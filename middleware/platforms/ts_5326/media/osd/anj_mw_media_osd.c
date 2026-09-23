#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"

#include "anj_mw_media_common.h"
#include "anj_mw_media_osd.h"

#include <pthread.h>
#include <stdlib.h>

#define TS_OSD_MAX_HANDLE (512)
#define TS_OSD_CANVAS_NUM (1)

#define ANJ_MW_OSD_DRAW_RGN_BASE (MAX_VIDEO_NUM * OSD_TYPE_MAX + MAX_RGN_MASK_AREA * MAX_VIDEO_NUM)
#define ANJ_MW_OSD_LINE_RGN_HANDLE(stream) (ANJ_MW_OSD_DRAW_RGN_BASE + (stream))
#define ANJ_MW_OSD_RECT_RGN_HANDLE(stream) (ANJ_MW_OSD_DRAW_RGN_BASE + MAX_VIDEO_NUM + (stream))

#define TS_OSD_RGN_VIRT(ci) ((void *)(TS_UINTPTR_T)((ci).u64VirtAddr))

static pthread_mutex_t s_stMutexOsdUptState = PTHREAD_MUTEX_INITIALIZER;
static unsigned char s_box_ready[MAX_VIDEO_NUM] = {0};

typedef struct
{
    TS_U16 u16X;
    TS_U16 u16Y;
} TsOsdDrawPoint_t;

typedef struct
{
    PIXEL_FORMAT_E ePixelFmt;
    TS_U32 u32Color;
} TsOsdDrawColor_t;

static PIXEL_FORMAT_E anj_mw_osd_pixel_fmt_from_app(int rgn_pixel_format)
{
    switch ((rgn_pixel_format_e)rgn_pixel_format)
    {
    case PIXEL_FORMAT_I2:
        return PIXEL_FORMAT_ARGB_2BIT;
    case PIXEL_FORMAT_I4:
        return PIXEL_FORMAT_ARGB_4BIT;
    case PIXEL_FORMAT_ARGB1555:
        return PIXEL_FORMAT_ARGB_1555;
    default:
        return PIXEL_FORMAT_ARGB_2BIT;
    }
}

static void TsOsdDrawPoint(void *pBaseAddr, TS_U32 u32Stride, TS_U32 u32Height, TS_U32 u32Width, TsOsdDrawPoint_t stPt,
                           TsOsdDrawColor_t stColor)
{
    if (pBaseAddr == NULL)
    {
        return;
    }

    if (stPt.u16X >= u32Width || stPt.u16Y >= u32Height)
    {
        return;
    }

    switch (stColor.ePixelFmt)
    {
    case PIXEL_FORMAT_ARGB_2BIT:
    case PIXEL_FORMAT_ARGB_2BPP:
    {
        TS_U32 stride_bytes = u32Stride;
        TS_U32 byte_pos = (TS_U32)stPt.u16Y * stride_bytes + (stPt.u16X / 4);
        if (stride_bytes == 0 || byte_pos >= stride_bytes * u32Height)
        {
            return;
        }
        TS_U8 *pCur = (TS_U8 *)pBaseAddr + byte_pos;
        TS_U8 shift = (TS_U8)((stPt.u16X % 4) * 2);
        *pCur = (TS_U8)((*pCur & ~(0x03u << shift)) | ((stColor.u32Color & 0x03u) << shift));
    }
    break;
    case PIXEL_FORMAT_ARGB_4BIT:
    {
        TS_U32 stride_bytes = u32Stride;
        TS_U32 byte_pos = (TS_U32)stPt.u16Y * stride_bytes + (stPt.u16X / 2);
        TS_U8 *pCur = (TS_U8 *)pBaseAddr + byte_pos;
        if (stPt.u16X % 2)
        {
            *pCur = (TS_U8)((*pCur & 0x0Fu) | ((stColor.u32Color & 0x0Fu) << 4));
        }
        else
        {
            *pCur = (TS_U8)((*pCur & 0xF0u) | (stColor.u32Color & 0x0Fu));
        }
    }
    break;
    case PIXEL_FORMAT_ARGB_1555:
    {
        TS_U32 stride_bytes = u32Stride;
        TS_U32 byte_pos = (TS_U32)stPt.u16Y * stride_bytes + (TS_U32)stPt.u16X * 2;
        TS_U8 *pCur = (TS_U8 *)pBaseAddr + byte_pos;
        *pCur = (TS_U8)((stColor.u32Color >> 8) & 0xFF);
        *(pCur + 1) = (TS_U8)(stColor.u32Color & 0xFF);
    }
    break;
    default:
        break;
    }
}

static void TsOsdDrawLine(void *pBaseAddr, TS_U32 u32Stride, TS_U32 u32Height, TS_U32 u32Width, TsOsdDrawPoint_t stStartPt,
                          TsOsdDrawPoint_t stEndPt, TS_U8 u8BorderWidth, TsOsdDrawColor_t stColor)
{
    TS_S16 x0 = (TS_S16)stStartPt.u16X;
    TS_S16 y0 = (TS_S16)stStartPt.u16Y;
    TS_S16 x1 = (TS_S16)stEndPt.u16X;
    TS_S16 y1 = (TS_S16)stEndPt.u16Y;

    TS_S16 width_left = 0;
    TS_S16 width_right = 0;
    if ((u8BorderWidth % 2) != 0)
    {
        width_left = (TS_S16)(u8BorderWidth >> 1);
        width_right = (TS_S16)((u8BorderWidth >> 1) + 1);
    }
    else
    {
        width_left = (TS_S16)(u8BorderWidth >> 1);
        width_right = width_left;
    }

    if (y0 == y1)
    {
        y0 = (TS_S16)CLAMP(y0, width_left, (TS_S16)(u32Height - width_right));
        y1 = y0;
    }
    if (x0 == x1)
    {
        x0 = (TS_S16)CLAMP(x0, width_left, (TS_S16)(u32Width - width_right));
        x1 = x0;
    }

    TS_S16 dx = (TS_S16)abs(x1 - x0);
    TS_S16 dy = (TS_S16)abs(y1 - y0);
    TS_BOOL bSteep = (dy > dx) ? TS_TRUE : TS_FALSE;

    if (x0 > x1)
    {
        SWAP(x0, x1);
        SWAP(y0, y1);
    }

    int deltax = x1 - x0;
    int deltay = y1 - y0;
    if (bSteep == TS_FALSE)
    {
        float y = y0;
        float ystep = (deltax != 0) ? ((float)deltay / (float)deltax) : 0.0f;

        for (TS_S16 x = x0; x <= x1; x++)
        {
            int pointY = (int)y;

            for (TS_S16 jIndex = (TS_S16)(0 - width_left); jIndex < width_right; jIndex++)
            {
                pointY = (int)y + jIndex;
                if (pointY < 0)
                {
                    continue;
                }
                if (pointY >= (int)u32Height)
                {
                    pointY = (int)u32Height - 1;
                }
                TsOsdDrawPoint_t stPt = {0};
                stPt.u16X = (TS_U16)x;
                stPt.u16Y = (TS_U16)pointY;
                TsOsdDrawPoint(pBaseAddr, u32Stride, u32Height, u32Width, stPt, stColor);
            }
            y += ystep;
        }
    }
    else
    {
        if (y0 > y1)
        {
            SWAP(y0, y1);
            SWAP(x0, x1);
        }
        deltax = x1 - x0;
        deltay = y1 - y0;
        float x = x0;
        float xstep = (deltay != 0) ? ((float)deltax / (float)deltay) : 0.0f;

        for (int y = y0; y < y1; y++)
        {
            int pointX = (int)x;
            for (int jIndex = (int)(0 - width_left); jIndex < width_right; jIndex++)
            {
                pointX = (int)x + jIndex;
                if (pointX < 0)
                {
                    continue;
                }
                if (pointX >= (int)u32Width)
                {
                    pointX = (int)u32Width - 1;
                }
                TsOsdDrawPoint_t stPt = {0};
                stPt.u16X = (TS_U16)pointX;
                stPt.u16Y = (TS_U16)y;
                TsOsdDrawPoint(pBaseAddr, u32Stride, u32Height, u32Width, stPt, stColor);
            }
            x += xstep;
        }
    }
}

static int anj_mw_osd_draw_canvas(RGN_CANVAS_INFO_S *pstRgnCanvasInfo, overlay_param_s *overlay_param)
{
    if (overlay_param == NULL || overlay_param->bitmap_data == NULL || pstRgnCanvasInfo == NULL ||
        pstRgnCanvasInfo->u64VirtAddr == 0)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    void *virt = TS_OSD_RGN_VIRT(*pstRgnCanvasInfo);

    int tmp_x = overlay_param->pos_x;
    int tmp_y = overlay_param->pos_y;
    int tmp_w = overlay_param->width;
    int tmp_h = overlay_param->height;
    TS_U32 u32Stride = pstRgnCanvasInfo->u32Stride;

    int alignment = 1;
    switch (pstRgnCanvasInfo->enPixelFmt)
    {
    case PIXEL_FORMAT_ARGB_2BIT:
    case PIXEL_FORMAT_ARGB_2BPP:
        alignment = 4;
        break;
    case PIXEL_FORMAT_ARGB_4BIT:
        alignment = 2;
        break;
    default:
        alignment = 1;
        break;
    }

    int src_width = overlay_param->width;
    int src_aligned_width = (int)ANJ_ALIGN_UP((TS_U32)src_width, (TS_U32)alignment);
    int dst_aligned_width = (int)ANJ_ALIGN_UP(pstRgnCanvasInfo->stSize.u32Width, (TS_U32)alignment);

    if ((int)pstRgnCanvasInfo->stSize.u32Width == ANJ_ALIGN_UP(overlay_param->width, 8) &&
        (int)pstRgnCanvasInfo->stSize.u32Height == ANJ_ALIGN_UP(overlay_param->height, 8))
    {
        tmp_x = 0;
        tmp_y = 0;
    }
    else
    {
        tmp_x = overlay_param->pos_x;
        tmp_y = overlay_param->pos_y;
    }

    int dst_bytes_per_pixel = 1;
    switch (pstRgnCanvasInfo->enPixelFmt)
    {
    case PIXEL_FORMAT_ARGB_2BIT:
    case PIXEL_FORMAT_ARGB_2BPP:
        tmp_x = tmp_x >> 2;
        tmp_w = src_aligned_width >> 2;
        dst_bytes_per_pixel = 0;
        break;
    case PIXEL_FORMAT_ARGB_4BIT:
        tmp_x = tmp_x >> 1;
        tmp_w = src_aligned_width >> 1;
        dst_bytes_per_pixel = 0;
        break;
    case PIXEL_FORMAT_ARGB_1555:
        tmp_x *= 2;
        tmp_w *= 2;
        dst_bytes_per_pixel = 2;
        break;
    default:
        __ERR("Unsupported pixel format: %d\n", pstRgnCanvasInfo->enPixelFmt);
        return TS_FAILURE;
    }

    if (tmp_x < 0 || tmp_y < 0 || tmp_x + tmp_w > dst_aligned_width || tmp_y + tmp_h > (int)pstRgnCanvasInfo->stSize.u32Height)
    {
        __ERR("Canvas area out of bound: (%d,%d)-(%d,%d) vs (%d,%d)\n", tmp_x, tmp_y, tmp_w, tmp_h, dst_aligned_width,
              pstRgnCanvasInfo->stSize.u32Height);
        return TS_FAILURE;
    }

    TS_U32 byte_pos = (TS_U32)tmp_y * u32Stride + (TS_U32)tmp_x;
    TS_U8 *pCur = (TS_U8 *)virt + byte_pos;
    TS_U8 *u8SrcBuf = (TS_U8 *)overlay_param->bitmap_data;

    for (int h = 0; h < tmp_h; h++)
    {
        if ((pCur + tmp_w > (TS_U8 *)virt + u32Stride * pstRgnCanvasInfo->stSize.u32Height) ||
            (u8SrcBuf + tmp_w >
             (TS_U8 *)overlay_param->bitmap_data + src_aligned_width * overlay_param->height * (dst_bytes_per_pixel ? 2 : 1)))
        {
            __WARN("Memory copy out of bound at line %d\n", h);
            break;
        }
        memcpy(pCur, u8SrcBuf, (size_t)tmp_w);
        pCur += u32Stride;
        u8SrcBuf += tmp_w;
    }

    return TS_SUCCESS;
}

static void anj_mw_osd_fill_overlay_chn_attr(RGN_CHN_ATTR_S *pstChnAttr, TS_S32 s32PosX, TS_S32 s32PosY, TS_U32 u32Layer,
                                             PIXEL_FORMAT_E enPix)
{
    memset(pstChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    pstChnAttr->bShow = TS_TRUE;
    pstChnAttr->enType = OVERLAY_RGN;
    pstChnAttr->unChnAttr.stOverlayChn.stPoint.s32X = s32PosX;
    pstChnAttr->unChnAttr.stOverlayChn.stPoint.s32Y = s32PosY;
    pstChnAttr->unChnAttr.stOverlayChn.u32FgAlpha = 0;
    pstChnAttr->unChnAttr.stOverlayChn.u32BgAlpha = 0;
    pstChnAttr->unChnAttr.stOverlayChn.u32Layer = u32Layer;
    pstChnAttr->unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
    pstChnAttr->unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
    pstChnAttr->unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUM_THRESH;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.u8InvertColorTh = 100;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.u8InvertColorBige = 0;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.u8InvertColorOsdThH = 0;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.u8InvertColorOsdThL = 0xff;
    pstChnAttr->unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TS_FALSE;

    pstChnAttr->unChnAttr.stOverlayChn.enAttachDest = ATTACH_JPEG_MAIN;

    if (enPix == PIXEL_FORMAT_ARGB_2BIT || enPix == PIXEL_FORMAT_ARGB_2BPP || enPix == PIXEL_FORMAT_ARGB_4BIT)
    {
        // 1bit lut
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8Alpha = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueR = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueG = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueB = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8Alpha = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueR = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueG = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueB = 0xff;

        // 2bit lut
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8Alpha = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueR = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueG = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueB = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8Alpha = 0xff;
        // index=1 => BLACK
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueR = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueG = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueB = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8Alpha = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueR = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueG = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueB = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8Alpha = 0xff;
        // index=3 => GREEN
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueR = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueG = 0xff;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueB = 0;

        // 4bit lut
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8Alpha = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueR = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueG = 0;
        pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueB = 0;
        for (int index = 1; index < 16; index++)
        {
            if (index == 1)
            {
                // BLACK
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0;
            }
            else if (index == 2)
            {
                // WHITE
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0xff;
            }
            else if (index == 3)
            {
                // GREEN
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0xff;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0;
            }
            else
            {
                // 其它 index 不使用时保持透明，避免把脏数据映射成可见边框
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0;
                pstChnAttr->unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0;
            }
        }
    }
}

static int anj_mw_osd_handle_valid(RGN_HANDLE h)
{
    if (h >= TS_OSD_MAX_HANDLE)
    {
        return TS_FALSE;
    }
    return TS_TRUE;
}

int anj_mw_osd_init(int pixel_fmt)
{
    int stream;
    int i;

    (void)pixel_fmt;

    anj_mutex_lock(&s_stMutexOsdUptState);

    // Initialize the vpss box (VPE)
    for (stream = 0; stream < MAX_VIDEO_NUM; stream++)
    {
        VPSS_GRP grp = (VPSS_GRP)(stream / MAX_VENC_CHN);
        VPSS_CHN chn = (VPSS_CHN)(stream % MAX_VENC_CHN);
        VPSS_BOX_ATTR_BATCH_S stBatch;

        memset(&stBatch, 0, sizeof(stBatch));
        for (i = 0; i < VPSS_BOXS_MAX_NUM; i++)
        {
            stBatch.astBoxAttr[i].handle = (BOX_HANDLE)i;
            stBatch.astBoxAttr[i].stBoxAttr.u32Status = 2;
        }
        if (TS_MPI_VPSS_CreateChnBoxAttrBatch(grp, chn, &stBatch) != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_CreateChnBoxAttrBatch fail grp=%d chn=%d\n", (int)grp, (int)chn);
            continue;
        }
        s_box_ready[stream] = 1;
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

int anj_mw_osd_uninit(void)
{
    int stream;

    anj_mutex_lock(&s_stMutexOsdUptState);
    // Destroy the vpss box (VPE)
    for (stream = 0; stream < MAX_VIDEO_NUM; stream++)
    {
        if (s_box_ready[stream])
        {
            VPSS_GRP grp = (VPSS_GRP)(stream / MAX_VENC_CHN);
            VPSS_CHN chn = (VPSS_CHN)(stream % MAX_VENC_CHN);
            if (TS_MPI_VPSS_DestoryChnBoxAttrBatch(grp, chn) != TS_SUCCESS)
            {
                __ERR("TS_MPI_VPSS_DestoryChnBoxAttrBatch fail grp=%d chn=%d\n", (int)grp, (int)chn);
            }
            s_box_ready[stream] = 0;
        }
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

int anj_mw_osd_create(overlay_param_s *overlay_param, ANJ_SIZE_S *resolution, int rgn_pixel_format)
{
    TS_S32 s32Ret = TS_SUCCESS;
    MPP_CHN_S stMppChn;
    RGN_ATTR_S stRegion = {0};
    RGN_CHN_ATTR_S stChnAttr = {0};

    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)overlay_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __WARN("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_SUCCESS;
    }

    if (OSD_FULL_IMAGE)
    {
        if (hHandle != (RGN_HANDLE)(overlay_param->stream_type * OSD_TYPE_MAX))
        {
            return TS_SUCCESS;
        }
    }

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = overlay_param->stream_type;

    TS_U32 u32W;
    TS_U32 u32H;
    TS_S32 s32X;
    TS_S32 s32Y;
    TS_U32 u32Layer;

    if (OSD_FULL_IMAGE)
    {
        if (resolution == NULL)
        {
            __ERR("resolution is null\n");
            return TS_FAILURE;
        }
        u32W = ANJ_ALIGN_DOWN(resolution->u32Width, 2);
        u32H = ANJ_ALIGN_DOWN(resolution->u32Height, 2);
        s32X = 0;
        s32Y = 0;
        u32Layer = (TS_U32)(overlay_param->stream_type * OSD_TYPE_MAX);
    }
    else
    {
        u32W = ANJ_ALIGN_DOWN((TS_U32)overlay_param->width, 2);
        u32H = ANJ_ALIGN_DOWN((TS_U32)overlay_param->height, 2);
        s32X = overlay_param->pos_x;
        s32Y = overlay_param->pos_y;
        u32Layer = (TS_U32)overlay_param->rgn_handle;
    }

    PIXEL_FORMAT_E enPix = anj_mw_osd_pixel_fmt_from_app(rgn_pixel_format);
    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.enPixelFmt = enPix;
    stRegion.unAttr.stOverlay.u32BgColor = 0;
    stRegion.unAttr.stOverlay.stSize.u32Width = u32W;
    stRegion.unAttr.stOverlay.stSize.u32Height = u32H;
    stRegion.unAttr.stOverlay.u32CanvasNum = TS_OSD_CANVAS_NUM;
    anj_mw_osd_fill_overlay_chn_attr(&stChnAttr, s32X, s32Y, u32Layer, enPix);

    /* 与 canvas Get/Draw/Update 互斥，避免 rgn 建立/销毁与 TS_MPI_RGN_UpdateCanvas
     * 的内核 cache flush 并发（会触发 sgs_mi 内核 Oops）。*/
    anj_mutex_lock(&s_stMutexOsdUptState);
    s32Ret = TS_Common_OsdCreate(hHandle, &stRegion, &stMppChn, &stChnAttr);
    anj_mutex_unlock(&s_stMutexOsdUptState);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdCreate failed 0x%x h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return (int)s32Ret;
    }
    TS_Common_OsdSetBitmap(hHandle, enPix, u32W, u32H, &stMppChn);

    s32Ret = (TS_S32)anj_mw_osd_canvas_info_clear(overlay_param);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("anj_mw_osd_canvas_info_clear() error(%d)\n", (int)s32Ret);
    }

    __INFO("rgn %u create sucess!\n", (unsigned int)hHandle);
    return TS_SUCCESS;
}

int anj_mw_osd_destroy(overlay_param_s *overlay_param)
{
    TS_S32 s32Ret = TS_SUCCESS;
    MPP_CHN_S stMppChn;

    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)overlay_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    if (OSD_FULL_IMAGE && hHandle != (RGN_HANDLE)(overlay_param->stream_type * OSD_TYPE_MAX))
    {
        return TS_SUCCESS;
    }

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = overlay_param->stream_type;

    s32Ret = (TS_S32)anj_mw_osd_canvas_info_clear(overlay_param);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("anj_mw_osd_canvas_info_clear() error(%d)\n", (int)s32Ret);
    }

    /* MI_RGN_Destroy 会 munmap canvas；必须与持锁进行中的
     * GetCanvasInfo/DrawRect/UpdateCanvas（如 rm_vyuv 线程画检测框）互斥，
     * 否则内核对 canvas 做 cache flush 时映射被解除，触发 kernel Oops。*/
    anj_mutex_lock(&s_stMutexOsdUptState);
    s32Ret = TS_Common_OsdDestory(hHandle, &stMppChn);
    anj_mutex_unlock(&s_stMutexOsdUptState);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdDestory failed 0x%x h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return (int)s32Ret;
    }

    return TS_SUCCESS;
}

int anj_mw_osd_bitmap_data_free(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    if (overlay_param->bitmap_data)
    {
        anj_mw_free(overlay_param->bitmap_data);
        overlay_param->bitmap_data = NULL;
        overlay_param->bitmap_len = 0;
    }

    return TS_SUCCESS;
}

int anj_mw_osd_bitmap_data_malloc(rgn_pixel_format_e ePixelFmt, overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    int len = 0;
    switch (ePixelFmt)
    {
    case PIXEL_FORMAT_I2:
        len = overlay_param->height * overlay_param->width / 4;
        break;
    case PIXEL_FORMAT_I4:
        len = overlay_param->height * overlay_param->width / 2;
        break;
    case PIXEL_FORMAT_ARGB1555:
        len = overlay_param->height * overlay_param->width * 2;
        break;
    default:
        break;
    }

    if (overlay_param->bitmap_data)
    {
        if (len > overlay_param->bitmap_len)
        {
            overlay_param->bitmap_data = anj_mw_realloc(overlay_param->bitmap_data, (size_t)len);
            overlay_param->bitmap_len = len;
        }
    }
    else if (len > 0)
    {
        overlay_param->bitmap_data = anj_mw_malloc((size_t)len);
        overlay_param->bitmap_len = len;
    }

    if (overlay_param->bitmap_data == NULL || overlay_param->bitmap_len <= 0)
    {
        __ERR("bitmap_data[%d] is null\n", overlay_param->rgn_handle);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

int anj_mw_osd_draw_pixel(rgn_pixel_format_e ePixelFmt, unsigned short value, int offset_x, int offset_y,
                          overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    if (overlay_param->bitmap_data == NULL)
    {
        return TS_SUCCESS;
    }

    int canvas_w = overlay_param->width;
    int canvas_h = overlay_param->height;
    if (offset_x < 0 || offset_y < 0 || offset_x >= canvas_w || offset_y >= canvas_h)
    {
        __ERR("Coordinate out of range: (%d, %d) vs (%d x %d)\n", offset_x, offset_y, canvas_w, canvas_h);
        return TS_FAILURE;
    }

    int alignment = 1;
    switch (ePixelFmt)
    {
    case PIXEL_FORMAT_I2:
        alignment = 4;
        break;
    case PIXEL_FORMAT_I4:
        alignment = 2;
        break;
    case PIXEL_FORMAT_ARGB1555:
        alignment = 1;
        break;
    default:
        __ERR("Unsupported pixel format: %d\n", ePixelFmt);
        return TS_FAILURE;
    }

    int aligned_width = (int)ANJ_ALIGN_UP((TS_U32)canvas_w, (TS_U32)alignment);

    TS_U8 *pCur = (TS_U8 *)overlay_param->bitmap_data;
    switch (ePixelFmt)
    {
    case PIXEL_FORMAT_I2:
    {
        TS_U32 stride_bytes = (TS_U32)(aligned_width / 4);
        TS_U32 byte_pos = (TS_U32)offset_y * stride_bytes + (TS_U32)(offset_x / 4);
        if (byte_pos >= (TS_U32)(aligned_width * canvas_h))
        {
            break;
        }
        pCur = (TS_U8 *)overlay_param->bitmap_data + byte_pos;
        TS_U8 shift = (TS_U8)((offset_x % 4) * 2);
        *pCur = (TS_U8)((*pCur & ~(0x03u << shift)) | (((TS_U32)value & 0x03u) << shift));
        break;
    }
    case PIXEL_FORMAT_I4:
    {
        TS_U32 stride_bytes = (TS_U32)(aligned_width / 2);
        TS_U32 byte_pos = (TS_U32)offset_y * stride_bytes + (TS_U32)(offset_x / 2);
        if (byte_pos >= (TS_U32)(aligned_width * canvas_h / 2))
        {
            break;
        }
        pCur = (TS_U8 *)overlay_param->bitmap_data + byte_pos;
        if (offset_x % 2)
        {
            *pCur = (TS_U8)((*pCur & 0x0Fu) | (((TS_U32)value & 0x0Fu) << 4));
        }
        else
        {
            *pCur = (TS_U8)((*pCur & 0xF0u) | ((TS_U32)value & 0x0Fu));
        }
        break;
    }
    case PIXEL_FORMAT_ARGB1555:
    {
        TS_U32 stride_bytes = (TS_U32)(aligned_width * 2);
        TS_U32 byte_pos = (TS_U32)offset_y * stride_bytes + (TS_U32)offset_x * 2;
        if (byte_pos + 1 >= (TS_U32)(aligned_width * canvas_h * 2))
        {
            break;
        }
        pCur = (TS_U8 *)overlay_param->bitmap_data + byte_pos;
        *pCur = (TS_U8)((value >> 8) & 0xFF);
        *(pCur + 1) = (TS_U8)(value & 0xFF);
        break;
    }
    default:
        break;
    }

    return TS_SUCCESS;
}

int anj_mw_osd_canvas_update(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    if (overlay_param->bitmap_data == NULL)
    {
        return TS_SUCCESS;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)overlay_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    RGN_HANDLE idx = hHandle;
    if (OSD_FULL_IMAGE)
    {
        idx = (RGN_HANDLE)(overlay_param->stream_type * OSD_TYPE_MAX);
    }

    if (!OSD_FULL_IMAGE)
    {
        RGN_ATTR_S stRgnAttr;
        RGN_CHN_ATTR_S stDispAttr;
        MPP_CHN_S stMppChn;
        memset(&stRgnAttr, 0, sizeof(stRgnAttr));
        memset(&stDispAttr, 0, sizeof(stDispAttr));
        memset(&stMppChn, 0, sizeof(stMppChn));
        stMppChn.enModId = TS_ID_VENC;
        stMppChn.s32DevId = 0;
        stMppChn.s32ChnId = overlay_param->stream_type;

        if (TS_Common_OsdAttrGet(idx, &stRgnAttr) != TS_SUCCESS)
        {
            if (overlay_param->width <= 0 || overlay_param->height <= 0)
            {
                return TS_SUCCESS;
            }
            if (anj_mw_osd_create(overlay_param, NULL, PIXEL_FORMAT_I2) != TS_SUCCESS ||
                TS_Common_OsdAttrGet(idx, &stRgnAttr) != TS_SUCCESS)
            {
                __ERR("on-demand TS_Common_OsdCreate failed idx=%u\n", (unsigned int)idx);
                return TS_FAILURE;
            }
        }
        if (TS_Common_OsdDisplayAttrGet(idx, &stMppChn, &stDispAttr) != TS_SUCCESS)
        {
            __ERR("TS_Common_OsdDisplayAttrGet failed idx=%u\n", (unsigned int)idx);
            return TS_FAILURE;
        }

        // 与 create() 中的对齐保持一致：非全屏 mode 下 u32W/u32H 会按 8 对齐后写入 RGN。
        // 这里如果不对齐，stRgnAttr 永远不相等，会导致不必要的 destroy+create。
        TS_U32 expectedW = ANJ_ALIGN_UP((TS_U32)overlay_param->width, 8);
        TS_U32 expectedH = ANJ_ALIGN_UP((TS_U32)overlay_param->height, 8);
        if (stRgnAttr.unAttr.stOverlay.stSize.u32Width != expectedW ||
            stRgnAttr.unAttr.stOverlay.stSize.u32Height != expectedH ||
            stDispAttr.unChnAttr.stOverlayChn.stPoint.s32X != overlay_param->pos_x ||
            stDispAttr.unChnAttr.stOverlayChn.stPoint.s32Y != overlay_param->pos_y)
        {
            anj_mw_osd_destroy(overlay_param);
            anj_mw_osd_create(overlay_param, NULL, PIXEL_FORMAT_I2);
        }
    }

    RGN_CANVAS_INFO_S stCanvas = {0};
    anj_mutex_lock(&s_stMutexOsdUptState);
    RGN_CANVAS_INFO_S *pstCanvas = NULL;
    if (TS_Common_OsdCanvasGet(idx, &pstCanvas) != TS_SUCCESS || pstCanvas == NULL)
    {
        __ERR("TS_Common_OsdCanvasGet failed idx=%u\n", (unsigned int)idx);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    stCanvas = *pstCanvas;

    if (anj_mw_osd_draw_canvas(&stCanvas, overlay_param) != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }

    if (TS_Common_OsdCanvasUpdate(idx) != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdCanvasUpdate failed idx=%u\n", (unsigned int)idx);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }

    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

int anj_mw_osd_canvas_info_clear(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)overlay_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    RGN_HANDLE idx = OSD_FULL_IMAGE ? (RGN_HANDLE)(overlay_param->stream_type * OSD_TYPE_MAX) : hHandle;

    RGN_CANVAS_INFO_S stCanvas = {0};
    anj_mutex_lock(&s_stMutexOsdUptState);
    RGN_CANVAS_INFO_S *pstCanvas = NULL;
    if (TS_Common_OsdCanvasGet(idx, &pstCanvas) != TS_SUCCESS || pstCanvas == NULL)
    {
        __ERR("TS_Common_OsdCanvasGet failed idx=%u\n", (unsigned int)idx);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    stCanvas = *pstCanvas;

    void *virt = TS_OSD_RGN_VIRT(stCanvas);
    if (virt && stCanvas.u32Stride > 0 && stCanvas.stSize.u32Height > 0)
    {
        memset(virt, 0, (size_t)stCanvas.u32Stride * stCanvas.stSize.u32Height);
    }

    if (TS_Common_OsdCanvasUpdate(idx) != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdCanvasUpdate failed idx=%u\n", (unsigned int)idx);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }

    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

/* 与 module/media/inc/anj_osd.h RGB_VALUE_* 保持一致 */
static TS_U32 anj_mw_osd_box_color_to_rgb(int color)
{
    switch (color)
    {
    case 1: /* RGB_VALUE_BLACK */
        return 0x000000U;
    case 2: /* RGB_VALUE_WHITE */
        return 0xFFFFFFU;
    case 5: /* RGB_VALUE_BLUE */
        return 0x0000FFU;
    case 3: /* RGB_VALUE_GREEN */
    default:
        return 0x00FF00U;
    }
}

static void anj_mw_vpss_box_fill_batch(rect_param_s *rect_param, int cnt, VPSS_BOX_ATTR_BATCH_S *pstBatch)
{
    int i;

    memset(pstBatch, 0, sizeof(*pstBatch));
    for (i = 0; i < VPSS_BOXS_MAX_NUM; i++)
    {
        pstBatch->astBoxAttr[i].handle = (BOX_HANDLE)i;
        if ((rect_param != NULL) && (i < cnt) && (i < MAX_RECT_NUM) &&
            (rect_param->draw_rect[i].width > 0) && (rect_param->draw_rect[i].height > 0) &&
            (rect_param->draw_rect[i].u32Color != 0))
        {
            int color = rect_param->draw_rect[i].u32Color;
            pstBatch->astBoxAttr[i].stBoxAttr.u32Status = 1;
            pstBatch->astBoxAttr[i].stBoxAttr.u32StartX =
                (TS_U32)((rect_param->draw_rect[i].pos_x < 0) ? 0 : rect_param->draw_rect[i].pos_x);
            pstBatch->astBoxAttr[i].stBoxAttr.u32StartY =
                (TS_U32)((rect_param->draw_rect[i].pos_y < 0) ? 0 : rect_param->draw_rect[i].pos_y);
            pstBatch->astBoxAttr[i].stBoxAttr.u32Width = (TS_U32)rect_param->draw_rect[i].width;
            pstBatch->astBoxAttr[i].stBoxAttr.u32Height = (TS_U32)rect_param->draw_rect[i].height;
            pstBatch->astBoxAttr[i].stBoxAttr.u32Thick = (TS_U32)rect_param->u8BorderWidth;
            pstBatch->astBoxAttr[i].stBoxAttr.u32Rgb = anj_mw_osd_box_color_to_rgb(color);
        }
        else
        {
            pstBatch->astBoxAttr[i].stBoxAttr.u32Status = 2;
        }
    }
}

int anj_mw_osd_update_rect(rect_param_s *rect_param)
{
    VPSS_GRP grp;
    VPSS_CHN chn;
    VPSS_BOX_ATTR_BATCH_S stBatch;
    int cnt;
    int stream;

    if (rect_param == NULL)
    {
        __ERR("input is null or error\n");
        return TS_FAILURE;
    }

    stream = rect_param->stream_type;
    if ((stream < 0) || (stream >= MAX_VIDEO_NUM) || !s_box_ready[stream])
    {
        __ERR("vpss box not ready stream=%d\n", stream);
        return TS_FAILURE;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    grp = (VPSS_GRP)(stream / MAX_VENC_CHN);
    chn = (VPSS_CHN)(stream % MAX_VENC_CHN);
    cnt = rect_param->bShow ? rect_param->s32RectCnt : 0;
    if (cnt < 0)
    {
        cnt = 0;
    }
    if (cnt > MAX_RECT_NUM)
    {
        cnt = MAX_RECT_NUM;
    }

    anj_mw_vpss_box_fill_batch(rect_param, cnt, &stBatch);
    if (TS_MPI_VPSS_SetChnBoxAttrBatch(grp, chn, &stBatch) != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_SetChnBoxAttrBatch fail grp=%d chn=%d\n", (int)grp, (int)chn);
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

int anj_mw_osd_clean_rect(rect_param_s *rect_param)
{
    VPSS_GRP grp;
    VPSS_CHN chn;
    VPSS_BOX_ATTR_BATCH_S stBatch;
    int stream;

    if (rect_param == NULL)
    {
        __ERR("input is null or error\n");
        return TS_FAILURE;
    }

    stream = rect_param->stream_type;
    if ((stream < 0) || (stream >= MAX_VIDEO_NUM) || !s_box_ready[stream])
    {
        return TS_SUCCESS;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    grp = (VPSS_GRP)(stream / MAX_VENC_CHN);
    chn = (VPSS_CHN)(stream % MAX_VENC_CHN);
    anj_mw_vpss_box_fill_batch(NULL, 0, &stBatch);
    if (TS_MPI_VPSS_SetChnBoxAttrBatch(grp, chn, &stBatch) != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_SetChnBoxAttrBatch(hide) fail grp=%d chn=%d\n", (int)grp, (int)chn);
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);
    return TS_SUCCESS;
}

int anj_mw_osd_update_line(line_param_s *line_param)
{
    RGN_CANVAS_INFO_S stCanvas = {0};
    TsOsdDrawColor_t stColor;
    TsOsdDrawPoint_t stStartPt;
    TsOsdDrawPoint_t stEndPt;
    RGN_CANVAS_INFO_S *pstCanvas = NULL;
    RGN_HANDLE hHandle;
    void *virt = NULL;

    memset(&stColor, 0, sizeof(stColor));
    memset(&stStartPt, 0, sizeof(stStartPt));
    memset(&stEndPt, 0, sizeof(stEndPt));

    if (line_param == NULL)
    {
        __ERR("input is null or error\n");
        return TS_FAILURE;
    }

    if (line_param->bShow == 0)
    {
        return TS_SUCCESS;
    }

    hHandle = (RGN_HANDLE)line_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    if (TS_Common_OsdCanvasGet(hHandle, &pstCanvas) != TS_SUCCESS || pstCanvas == NULL)
    {
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    stCanvas = *pstCanvas;

    stColor.ePixelFmt = stCanvas.enPixelFmt;
    virt = TS_OSD_RGN_VIRT(stCanvas);

    for (int i = 0; i < line_param->s32LineCnt; i++)
    {
        stColor.u32Color = (TS_U32)line_param->u32Color[i];
        stStartPt.u16X = (TS_U16)line_param->stPoint[i].pos_x;
        stStartPt.u16Y = (TS_U16)line_param->stPoint[i].pos_y;
        stEndPt.u16X = (TS_U16)line_param->enPoint[i].pos_x;
        stEndPt.u16Y = (TS_U16)line_param->enPoint[i].pos_y;

        switch (stCanvas.enPixelFmt)
        {
        case PIXEL_FORMAT_ARGB_1555:
            break;
        case PIXEL_FORMAT_ARGB_4BIT:
            stStartPt.u16X = (TS_U16)ANJ_ALIGN_DOWN(stStartPt.u16X, 2);
            stStartPt.u16Y = (TS_U16)ANJ_ALIGN_DOWN(stStartPt.u16Y, 2);
            stEndPt.u16X = (TS_U16)ANJ_ALIGN_DOWN(stEndPt.u16X, 2);
            stEndPt.u16Y = (TS_U16)ANJ_ALIGN_DOWN(stEndPt.u16Y, 2);
            break;
        case PIXEL_FORMAT_ARGB_2BIT:
        case PIXEL_FORMAT_ARGB_2BPP:
            stStartPt.u16X = (TS_U16)ANJ_ALIGN_DOWN(stStartPt.u16X, 4);
            stStartPt.u16Y = (TS_U16)ANJ_ALIGN_DOWN(stStartPt.u16Y, 4);
            stEndPt.u16X = (TS_U16)ANJ_ALIGN_DOWN(stEndPt.u16X, 4);
            stEndPt.u16Y = (TS_U16)ANJ_ALIGN_DOWN(stEndPt.u16Y, 4);
            break;
        default:
            __ERR("unsupport %d pixel format!\n", stCanvas.enPixelFmt);
            break;
        }

        TsOsdDrawLine(virt, stCanvas.u32Stride, stCanvas.stSize.u32Height, stCanvas.stSize.u32Width, stStartPt, stEndPt,
                      line_param->u8BorderWidth, stColor);
    }

    if (TS_Common_OsdCanvasUpdate(hHandle) != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return TS_SUCCESS;
}

int anj_mw_osd_clean_line(line_param_s *line_param)
{
    RGN_CANVAS_INFO_S stCanvas = {0};
    RGN_CANVAS_INFO_S *pstCanvas = NULL;
    RGN_HANDLE hHandle;
    void *virt = NULL;

    if (line_param == NULL)
    {
        __ERR("input is null or error\n");
        return TS_FAILURE;
    }

    if (line_param->bShow == 0)
    {
        return TS_SUCCESS;
    }

    hHandle = (RGN_HANDLE)line_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    if (TS_Common_OsdCanvasGet(hHandle, &pstCanvas) != TS_SUCCESS || pstCanvas == NULL)
    {
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    stCanvas = *pstCanvas;

    virt = TS_OSD_RGN_VIRT(stCanvas);
    if (virt && stCanvas.u32Stride > 0 && stCanvas.stSize.u32Height > 0)
    {
        memset(virt, 0, (size_t)stCanvas.u32Stride * stCanvas.stSize.u32Height);
    }

    if (TS_Common_OsdCanvasUpdate(hHandle) != TS_SUCCESS)
    {
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return TS_FAILURE;
    }
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return TS_SUCCESS;
}

void anj_mw_osd_line_param_init(line_param_s *line_param, int stream_type, int stream_width, int stream_height)
{
    if (line_param == NULL)
    {
        return;
    }

    if (OSD_FULL_IMAGE)
    {
        line_param->rgn_handle = stream_type * OSD_TYPE_MAX;
    }
    else
    {
        line_param->rgn_handle = ANJ_MW_OSD_LINE_RGN_HANDLE(stream_type);
    }
    line_param->stream_type = stream_type;
    line_param->stream_width = stream_width;
    line_param->stream_height = stream_height;
}

void anj_mw_osd_rect_param_init(rect_param_s *rect_param, int stream_type, int stream_width, int stream_height)
{
    if (rect_param == NULL)
    {
        return;
    }

    if (OSD_FULL_IMAGE)
    {
        rect_param->rgn_handle = stream_type * OSD_TYPE_MAX;
    }
    else
    {
        rect_param->rgn_handle = ANJ_MW_OSD_RECT_RGN_HANDLE(stream_type);
    }
    rect_param->stream_type = stream_type;
    rect_param->stream_width = stream_width;
    rect_param->stream_height = stream_height;
}

int anj_mw_osd_draw_overlay_create(int stream_type, int stream_width, int stream_height, int rgn_handle)
{
    TS_S32 s32Ret = TS_SUCCESS;
    MPP_CHN_S stMppChn;
    RGN_ATTR_S stRegion;
    RGN_CHN_ATTR_S stChnAttr;
    RGN_HANDLE hHandle;
    PIXEL_FORMAT_E enPix = PIXEL_FORMAT_ARGB_2BIT;
    TS_U32 u32W = 0;
    TS_U32 u32H = 0;

    if (OSD_FULL_IMAGE)
    {
        return TS_SUCCESS;
    }

    if (stream_width <= 0 || stream_height <= 0)
    {
        return TS_SUCCESS;
    }

    hHandle = (RGN_HANDLE)rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD draw handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    u32W = ANJ_ALIGN_UP((TS_U32)stream_width, 8);
    u32H = ANJ_ALIGN_UP((TS_U32)stream_height, 8);

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = stream_type;

    memset(&stRegion, 0, sizeof(stRegion));
    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.enPixelFmt = enPix;
    stRegion.unAttr.stOverlay.u32BgColor = 0;
    stRegion.unAttr.stOverlay.stSize.u32Width = u32W;
    stRegion.unAttr.stOverlay.stSize.u32Height = u32H;
    stRegion.unAttr.stOverlay.u32CanvasNum = TS_OSD_CANVAS_NUM;
    anj_mw_osd_fill_overlay_chn_attr(&stChnAttr, 0, 0, (TS_U32)hHandle, enPix);

    s32Ret = TS_Common_OsdCreate(hHandle, &stRegion, &stMppChn, &stChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdCreate(draw) failed 0x%x h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return (int)s32Ret;
    }

    TS_Common_OsdSetBitmap(hHandle, enPix, u32W, u32H, &stMppChn);
    __INFO("draw overlay handle:%u stream:%d %ux%u create sucess!\n", (unsigned int)hHandle, stream_type, (unsigned int)u32W,
           (unsigned int)u32H);
    return TS_SUCCESS;
}

int anj_mw_osd_draw_overlay_destroy(int stream_type, int rgn_handle)
{
    TS_S32 s32Ret;
    MPP_CHN_S stMppChn;
    RGN_HANDLE hHandle;

    if (OSD_FULL_IMAGE)
    {
        return TS_SUCCESS;
    }

    hHandle = (RGN_HANDLE)rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        return TS_SUCCESS;
    }

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VENC;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = stream_type;

    s32Ret = TS_Common_OsdDestory(hHandle, &stMppChn);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdDestory(draw) failed 0x%x h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return (int)s32Ret;
    }

    return TS_SUCCESS;
}

int anj_mw_osd_cover_create(cover_param_s *cover_param)
{
    TS_S32 s32Ret = TS_SUCCESS;
    MPP_CHN_S stMppChn;
    RGN_ATTR_S stRegion;
    RGN_CHN_ATTR_S stChnAttr;

    if (cover_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)cover_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD cover handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VPSS;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = cover_param->stream_type;

    if (cover_param->cover_rect.width * cover_param->cover_rect.height <= 0)
    {
        return TS_SUCCESS;
    }

    memset(&stRegion, 0, sizeof(stRegion));
    stRegion.enType = COVER_RGN;

    memset(&stChnAttr, 0, sizeof(stChnAttr));
    stChnAttr.bShow = TS_TRUE;
    stChnAttr.enType = COVER_RGN;
    stChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;
    stChnAttr.unChnAttr.stCoverChn.stRect.s32X = cover_param->cover_rect.pos_x;
    stChnAttr.unChnAttr.stCoverChn.stRect.s32Y = cover_param->cover_rect.pos_y;
    stChnAttr.unChnAttr.stCoverChn.stRect.u32Width = (TS_U32)cover_param->cover_rect.width;
    stChnAttr.unChnAttr.stCoverChn.stRect.u32Height = (TS_U32)cover_param->cover_rect.height;
    stChnAttr.unChnAttr.stCoverChn.u32Color = cover_param->cover_rect.u32Color;
    stChnAttr.unChnAttr.stCoverChn.u32Layer = 0;
    stChnAttr.unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR;

    s32Ret = TS_Common_OsdCreate(hHandle, &stRegion, &stMppChn, &stChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdCreate(cover) failed 0x%x\n", (unsigned int)s32Ret);
        return (int)s32Ret;
    }

    usleep(100 * 1000);
    __INFO("cover handle:%u xy:%d %d wd:%d %d create sucess!\n", (unsigned int)hHandle, cover_param->cover_rect.pos_x,
           cover_param->cover_rect.pos_y, cover_param->cover_rect.width, cover_param->cover_rect.height);

    return TS_SUCCESS;
}

int anj_mw_osd_cover_destroy(cover_param_s *cover_param)
{
    TS_S32 s32Ret;
    MPP_CHN_S stMppChn;

    if (cover_param == NULL)
    {
        __ERR("input is null\n");
        return TS_FAILURE;
    }

    RGN_HANDLE hHandle = (RGN_HANDLE)cover_param->rgn_handle;
    if (!anj_mw_osd_handle_valid(hHandle))
    {
        __ERR("OSD cover handle error,hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    memset(&stMppChn, 0, sizeof(stMppChn));
    stMppChn.enModId = TS_ID_VPSS;
    stMppChn.s32DevId = 0;
    stMppChn.s32ChnId = cover_param->stream_type;

    s32Ret = TS_Common_OsdDestory(hHandle, &stMppChn);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_Common_OsdDestory(cover) failed 0x%x\n", (unsigned int)s32Ret);
        return (int)s32Ret;
    }

    return TS_SUCCESS;
}

