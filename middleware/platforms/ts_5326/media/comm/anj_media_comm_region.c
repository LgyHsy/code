#if 0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include "ts_common.h"
#include "anj_media_comm_region.h"
#include "osd/osd_truetype.h"
#include "osd/osd_timestamp.h"
#include "soft_line.h"
#include "osd/nv12_calcbright.h"

#define OverlayMinHandle 0
#define OverlayExMinHandle 20
#define CoverMinHandle 40
#define CoverExMinHandle 60
#define MosaicMinHandle 80
#define MosaicExMinHandle 100

TS_CHAR *g_pathBMP = TS_NULL;
static RGN_HANDLE g_ffHdlExt[10] = {0, 3};

TS_S32 REGION_MST_LoadBmp(const TS_CHAR *filename, BITMAP_S *pstBitmap, TS_BOOL bFil, TS_U32 u16FilColor,
                          PIXEL_FORMAT_E enPixelFormat)
{
    OSD_SURFACE_S stSurface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;
    TS_S32 s32BytesPerPix = 2;
    TS_U8 *pu8Data;
    TS_S32 s32RValue;
    TS_S32 s32GValue;
    TS_S32 s32BValue;
    TS_S32 s32GrValue;
    TS_U8 u8ValueTmp;
    TS_U8 u8Value;
    TS_S32 s32Width;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0) {
        printf("GetBmpInfo err!\n");
        return TS_FAILURE;
    }

    if (enPixelFormat == PIXEL_FORMAT_ARGB_4444) {
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_1555 || enPixelFormat == PIXEL_FORMAT_ARGB_2BPP) {
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_8888) {
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB8888;
        s32BytesPerPix = 4;
    } else {
        printf("enPixelFormat err %d\n", enPixelFormat);
        return TS_FAILURE;
    }

    pstBitmap->pData = malloc(s32BytesPerPix * (bmpInfo.bmiHeader.biWidth) * (bmpInfo.bmiHeader.biHeight));

    if (NULL == pstBitmap->pData) {
        printf("malloc osd memroy err!\n");
        return TS_FAILURE;
    }

    CreateSurfaceByBitMap(filename, &stSurface, (TS_U8 *)(pstBitmap->pData));

    pstBitmap->u32Width = stSurface.u16Width;
    pstBitmap->u32Height = stSurface.u16Height;
    pstBitmap->enPixelFormat = enPixelFormat;

    TS_S32 i, j, k;
    TS_U8 *pu8Temp;

    if (PIXEL_FORMAT_ARGB_2BPP == enPixelFormat) {
        s32Width = DIV_UP(bmpInfo.bmiHeader.biWidth, 4);
        pu8Data = malloc((s32Width) * (bmpInfo.bmiHeader.biHeight));
        if (NULL == pu8Data) {
            printf("malloc osd memroy err!\n");
            return TS_FAILURE;
        }
    }
    if (PIXEL_FORMAT_ARGB_2BPP != enPixelFormat) {
        TS_U16 *pu16Temp;

        pu16Temp = (TS_U16 *)pstBitmap->pData;

        if (bFil) {
            for (i = 0; i < pstBitmap->u32Height; i++) {
                for (j = 0; j < pstBitmap->u32Width; j++) {
                    if (u16FilColor == *pu16Temp)
                        *pu16Temp &= 0x7FFF;

                    pu16Temp++;
                }
            }
        }
    } else {
        TS_U16 *pu16Temp;

        pu16Temp = (TS_U16 *)pstBitmap->pData;
        pu8Temp = (TS_U8 *)pu8Data;
        for (i = 0; i < pstBitmap->u32Height; i++) {
            for (j = 0; j < pstBitmap->u32Width / 4; j++) {
                u8Value = 0;
                for (k = j; k < j + 4; k++) {
                    s32BValue = *pu16Temp & 0x001F;
                    s32GValue = *pu16Temp >> 5 & 0x001F;
                    s32RValue = *pu16Temp >> 10 & 0x001F;
                    pu16Temp++;
                    s32GrValue = (s32RValue * 299 + s32GValue * 587 + s32BValue * 144 + 500) / 1000;
                    if (s32GrValue > 16)
                        u8ValueTmp = 0x01;
                    else
                        u8ValueTmp = 0x00;
                    u8Value = (u8Value << 2) + u8ValueTmp;
                }
                *pu8Temp = u8Value;
                pu8Temp++;
            }
        }
        free(pstBitmap->pData);
        pstBitmap->pData = pu8Data;
    }

    return TS_SUCCESS;
}

TS_S32 REGION_MST_UpdateCanvas(const TS_CHAR *filename, BITMAP_S *pstBitmap, TS_BOOL bFil, TS_U32 u16FilColor,
                               SIZE_S *pstSize, TS_U32 u32Stride, PIXEL_FORMAT_E enPixelFmt)
{
    OSD_SURFACE_S stSurface;
    OSD_BITMAPFILEHEADER stBmpFileHeader;
    OSD_BITMAPINFO stBmpInfo;

    if (GetBmpInfo(filename, &stBmpFileHeader, &stBmpInfo) < 0) {
        printf("GetBmpInfo err!\n");
        return TS_FAILURE;
    }

    if (PIXEL_FORMAT_ARGB_1555 == enPixelFmt)
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    else if (PIXEL_FORMAT_ARGB_4444 == enPixelFmt)
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    else if (PIXEL_FORMAT_ARGB_8888 == enPixelFmt)
        stSurface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    else {
        printf("Pixel format is not support!\n");
        return TS_FAILURE;
    }

    if (NULL == pstBitmap->pData) {
        printf("malloc osd memroy err!\n");
        return TS_FAILURE;
    }

    CreateSurfaceByCanvas(filename, &stSurface, (TS_U8 *)(pstBitmap->pData), pstSize->u32Width, pstSize->u32Height,
                          u32Stride);

    pstBitmap->u32Width = stSurface.u16Width;
    pstBitmap->u32Height = stSurface.u16Height;

    if (PIXEL_FORMAT_ARGB_1555 == enPixelFmt)
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_1555;
    else if (PIXEL_FORMAT_ARGB_4444 == enPixelFmt)
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_4444;
    else if (PIXEL_FORMAT_ARGB_8888 == enPixelFmt)
        pstBitmap->enPixelFormat = PIXEL_FORMAT_ARGB_8888;

    TS_S32 i, j;
    TS_U16 *pu16Temp;

    pu16Temp = (TS_U16 *)pstBitmap->pData;
    if (bFil) {
        for (i = 0; i < pstBitmap->u32Height; i++) {
            for (j = 0; j < pstBitmap->u32Width; j++) {
                if (u16FilColor == *pu16Temp)
                    *pu16Temp &= 0x7FFF;

                pu16Temp++;
            }
        }
    }

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_GetMinHandle(RGN_TYPE_E enType)
{
    TS_S32 s32MinHandle;

    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        break;
    default:
        s32MinHandle = -1;
        break;
    }
    return s32MinHandle;
}

TS_S32 SAMPLE_REGION_CreateOverLay(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = OVERLAY_RGN;
    stRegion.unAttr.stOverlay.enPixelFmt = PIXEL_FORMAT_ARGB_8888;
    stRegion.unAttr.stOverlay.stSize.u32Height = 144;
    stRegion.unAttr.stOverlay.stSize.u32Width = 180;
    stRegion.unAttr.stOverlay.u32BgColor = 0x00ff00ff;
    stRegion.unAttr.stOverlay.u32CanvasNum = 2;
    for (i = OverlayMinHandle; i < HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_CreateOverLayEx(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = OVERLAYEX_RGN;
    stRegion.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_ARGB_8888;
    stRegion.unAttr.stOverlayEx.stSize.u32Height = 144;
    stRegion.unAttr.stOverlayEx.stSize.u32Width = 180;
    stRegion.unAttr.stOverlayEx.u32BgColor = 0x00ff00ff;
    stRegion.unAttr.stOverlayEx.u32CanvasNum = 2;
    for (i = OverlayExMinHandle; i < OverlayExMinHandle + HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_CreateCover(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = COVER_RGN;

    for (i = CoverMinHandle; i < CoverMinHandle + HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_CreateCoverEx(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = COVEREX_RGN;

    for (i = CoverExMinHandle; i < CoverExMinHandle + HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_CreateMosaic(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = MOSAIC_RGN;

    for (i = MosaicMinHandle; i < MosaicMinHandle + HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_CreateMosaicEx(TS_S32 HandleNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_ATTR_S stRegion;

    stRegion.enType = MOSAICEX_RGN;

    for (i = MosaicExMinHandle; i < MosaicExMinHandle + HandleNum; i++) {
        s32Ret = TS_MPI_RGN_Create(i, &stRegion);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    return s32Ret;
}

TS_S32 SAMPLE_REGION_Destroy(RGN_HANDLE Handle)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_Destroy(Handle);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_Destroy failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_SetAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRegion)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_SetAttr(Handle, pstRegion);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_SetAttr failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_GetAttr(RGN_HANDLE Handle, RGN_ATTR_S *pstRegion)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_Create(Handle, pstRegion);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_Create failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_AttachToChn(RGN_HANDLE Handle, MPP_CHN_S *pstChn, RGN_CHN_ATTR_S *pstChnAttr)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_AttachToChn(Handle, pstChn, pstChnAttr);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_AttachToChn failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_DetachFromChn(RGN_HANDLE Handle, MPP_CHN_S *pstChn)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_DetachFromChn(Handle, pstChn);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_DetachFromChn failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_SetDisplayAttr(RGN_HANDLE Handle, MPP_CHN_S *pstChn, RGN_CHN_ATTR_S *pstChnAttr)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_SetDisplayAttr(Handle, pstChn, pstChnAttr);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_SetDisplayAttr failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_GetDisplayAttr(RGN_HANDLE Handle, MPP_CHN_S *pstChn, RGN_CHN_ATTR_S *pstChnAttr)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_GetDisplayAttr(Handle, pstChn, pstChnAttr);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_GetDisplayAttr failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_SetBitMap(RGN_HANDLE Handle, BITMAP_S *pstBitmap)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_SetBitMap(Handle, pstBitmap);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_SetBitMap failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_REGION_GetUpCanvasInfo(RGN_HANDLE Handle, RGN_CANVAS_INFO_S *pstCanvasInfo)
{
    TS_S32 s32Ret;

    s32Ret = TS_MPI_RGN_GetCanvasInfo(Handle, pstCanvasInfo);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_GetCanvasInfo failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_RGN_UpdateCanvas(Handle);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_UpdateCanvas failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_Create(TS_S32 HandleNum, RGN_TYPE_E enType)
{
    TS_S32 s32Ret;

    if (HandleNum <= 0 || HandleNum > 24) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    switch (enType) {
    case OVERLAY_RGN:
        s32Ret = SAMPLE_REGION_CreateOverLay(HandleNum);
        break;
    case OVERLAYEX_RGN:
        s32Ret = SAMPLE_REGION_CreateOverLayEx(HandleNum);
        break;
    case COVER_RGN:
        s32Ret = SAMPLE_REGION_CreateCover(HandleNum);
        break;
    case COVEREX_RGN:
        s32Ret = SAMPLE_REGION_CreateCoverEx(HandleNum);
        break;
    case MOSAIC_RGN:
        s32Ret = SAMPLE_REGION_CreateMosaic(HandleNum);
        break;
    case MOSAICEX_RGN:
        s32Ret = SAMPLE_REGION_CreateMosaicEx(HandleNum);
        break;
    default:
        break;
    }
    if (TS_SUCCESS != s32Ret) {
        __ERR("failed! HandleNum%d,entype:%d!\n", HandleNum, enType);
        return TS_FAILURE;
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_Destroy(TS_S32 HandleNum, RGN_TYPE_E enType)
{
    TS_S32 i;
    TS_S32 s32Ret = TS_SUCCESS;
    TS_S32 s32MinHandle;

    if (HandleNum <= 0 || HandleNum > 24) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        break;
    default:
        break;
    }
    for (i = s32MinHandle; i < s32MinHandle + HandleNum; i++) {
        s32Ret = SAMPLE_REGION_Destroy(i);
        if (TS_SUCCESS != s32Ret)
            __ERR("failed!\n");
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_AttachToChn(TS_S32 HandleNum, RGN_TYPE_E enType, MPP_CHN_S *pstMppChn)
{
    TS_S32 i;
    TS_S32 s32Ret;
    TS_S32 s32MinHandle;
    RGN_CHN_ATTR_S stChnAttr;

    if (HandleNum <= 0 || HandleNum > 16) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    if (TS_NULL == pstMppChn) {
        __ERR("pstMppChn is NULL !\n");
        return TS_FAILURE;
    }
    /*set the chn config*/
    stChnAttr.bShow = TS_TRUE;
    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAY_RGN;

        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;

        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;

        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUM_THRESH;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[0] = 0x2abc;
        stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[1] = 0x7FF0;
        stChnAttr.unChnAttr.stOverlayChn.enAttachDest = ATTACH_JPEG_MAIN;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAYEX_RGN;

        stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha = 128;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = COVER_RGN;
        stChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;

        stChnAttr.unChnAttr.stCoverChn.stRect.u32Height = 200;
        stChnAttr.unChnAttr.stCoverChn.stRect.u32Width = 200;

        stChnAttr.unChnAttr.stCoverChn.u32Color = 0x0000ffff;

        stChnAttr.unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = COVEREX_RGN;
        stChnAttr.unChnAttr.stCoverExChn.enCoverType = AREA_RECT;

        stChnAttr.unChnAttr.stCoverExChn.stRect.u32Height = 200;
        stChnAttr.unChnAttr.stCoverExChn.stRect.u32Width = 200;

        stChnAttr.unChnAttr.stCoverExChn.u32Color = 0x0000ffff;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        stChnAttr.enType = MOSAIC_RGN;
        stChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_32;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height = 200;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = 200;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        stChnAttr.enType = MOSAICEX_RGN;
        stChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_32;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height = 200;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = 200;
        break;
    default:
        break;
    }
    /*attach to Chn*/
    for (i = s32MinHandle; i < s32MinHandle + HandleNum; i++) {
        if (OVERLAY_RGN == enType) {
            stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 20 + 200 * (i - OverlayMinHandle);
            stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 20 + 200 * (i - OverlayMinHandle);
            stChnAttr.unChnAttr.stOverlayChn.u32Layer = i - OverlayMinHandle;
        }
        if (OVERLAYEX_RGN == enType) {
            stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 20 + 200 * (i - OverlayExMinHandle);
            stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 20 + 200 * (i - OverlayExMinHandle);
            stChnAttr.unChnAttr.stOverlayExChn.u32Layer = i - OverlayExMinHandle;
        }
        if (COVER_RGN == enType) {
            stChnAttr.unChnAttr.stCoverChn.stRect.s32X = 20 + 200 * (i - CoverMinHandle);
            stChnAttr.unChnAttr.stCoverChn.stRect.s32Y = 20 + 200 * (i - CoverMinHandle);
            stChnAttr.unChnAttr.stCoverChn.u32Layer = i - CoverMinHandle;
        }
        if (COVEREX_RGN == enType) {
            stChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 400 + 200 * (i - CoverExMinHandle);
            stChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 20 + 200 * (i - CoverExMinHandle);
            stChnAttr.unChnAttr.stCoverExChn.u32Layer = i - CoverExMinHandle;
        }
        if (MOSAIC_RGN == enType) {
            stChnAttr.unChnAttr.stMosaicChn.stRect.s32X = 400 + 200 * (i - MosaicMinHandle);
            stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = 20 + 200 * (i - MosaicMinHandle);
            stChnAttr.unChnAttr.stMosaicChn.u32Layer = i - MosaicMinHandle;
        }
        if (MOSAICEX_RGN == enType) {
            stChnAttr.unChnAttr.stMosaicChn.stRect.s32X = 800 + 200 * (i - MosaicExMinHandle);
            stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = 20 + 200 * (i - MosaicExMinHandle);
            stChnAttr.unChnAttr.stMosaicChn.u32Layer = i - MosaicExMinHandle;
        }
        s32Ret = SAMPLE_REGION_AttachToChn(i, pstMppChn, &stChnAttr);
        if (TS_SUCCESS != s32Ret) {
            __ERR("SAMPLE_REGION_AttachToChn failed!\n");
            break;
        }
    }
    /*detach region from chn */
    if (TS_SUCCESS != s32Ret && i > 0) {
        i--;
        for (; i >= s32MinHandle; i--)
            s32Ret = SAMPLE_REGION_DetachFromChn(i, pstMppChn);
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_AttachToMultiChn(TS_S32 HandleNum, RGN_TYPE_E enType, MOD_ID_E enModId, TS_S32 s32DevId,
                                           TS_S32 chnCnt)
{
    TS_S32 i;
    TS_S32 j;
    TS_S32 tmp_index = 0;
    TS_S32 s32Ret = 0;
    TS_S32 s32MinHandle;
    RGN_CHN_ATTR_S stChnAttr;
    MPP_CHN_S stChn = {0};
    MPP_CHN_S *pstMppChn = &stChn;

    pstMppChn->enModId = enModId;
    pstMppChn->s32DevId = s32DevId;
    pstMppChn->s32ChnId = 0;

    if (HandleNum <= 0 || HandleNum > 16) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    if (TS_NULL == pstMppChn) {
        __ERR("pstMppChn is NULL !\n");
        return TS_FAILURE;
    }
    /*set the chn config*/
    stChnAttr.bShow = TS_TRUE;
    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAY_RGN;

        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 128;

        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;

        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.enChgMod = LESSTHAN_LUM_THRESH;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[0] = 0x2abc;
        stChnAttr.unChnAttr.stOverlayChn.u16ColorLUT[1] = 0x7FF0;
        stChnAttr.unChnAttr.stOverlayChn.enAttachDest = ATTACH_JPEG_MAIN;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAYEX_RGN;

        stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha = 128;
        stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha = 128;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = COVER_RGN;
        stChnAttr.unChnAttr.stCoverChn.enCoverType = AREA_RECT;

        stChnAttr.unChnAttr.stCoverChn.stRect.u32Height = 100;
        stChnAttr.unChnAttr.stCoverChn.stRect.u32Width = 100;

        stChnAttr.unChnAttr.stCoverChn.u32Color = 0x0000ffff;

        stChnAttr.unChnAttr.stCoverChn.enCoordinate = RGN_ABS_COOR;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;

        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = COVEREX_RGN;
        stChnAttr.unChnAttr.stCoverExChn.enCoverType = AREA_RECT;

        stChnAttr.unChnAttr.stCoverExChn.stRect.u32Height = 100;
        stChnAttr.unChnAttr.stCoverExChn.stRect.u32Width = 100;

        stChnAttr.unChnAttr.stCoverExChn.u32Color = 0x0000ffff;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        stChnAttr.enType = MOSAIC_RGN;
        stChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_32;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height = 100;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = 100;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        stChnAttr.enType = MOSAICEX_RGN;
        stChnAttr.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_32;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Height = 100;
        stChnAttr.unChnAttr.stMosaicChn.stRect.u32Width = 100;
        break;
    default:
        break;
    }
    /*attach to Chn*/
    for (j = 0; j < chnCnt; j++) {
        pstMppChn->s32ChnId = j;
        for (i = s32MinHandle; i < s32MinHandle + HandleNum; i++) {
            if (OVERLAY_RGN == enType) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 20 + 100 * (i - OverlayMinHandle);
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 20 + 100 * (i - OverlayMinHandle);
                stChnAttr.unChnAttr.stOverlayChn.u32Layer = i - OverlayMinHandle;
            }
            if (OVERLAYEX_RGN == enType) {
                stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 20 + 100 * (i - OverlayExMinHandle);
                stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 20 + 100 * (i - OverlayExMinHandle);
                stChnAttr.unChnAttr.stOverlayExChn.u32Layer = i - OverlayExMinHandle;
            }
            if (COVER_RGN == enType) {
                stChnAttr.unChnAttr.stCoverChn.stRect.s32X = 20 + 100 * (i - CoverMinHandle);
                stChnAttr.unChnAttr.stCoverChn.stRect.s32Y = 20 + 100 * (i - CoverMinHandle);
                stChnAttr.unChnAttr.stCoverChn.u32Layer = i - CoverMinHandle;
            }
            if (COVEREX_RGN == enType) {
                stChnAttr.unChnAttr.stCoverExChn.stRect.s32X = 400 + 100 * (i - CoverExMinHandle);
                stChnAttr.unChnAttr.stCoverExChn.stRect.s32Y = 20 + 100 * (i - CoverExMinHandle);
                stChnAttr.unChnAttr.stCoverExChn.u32Layer = i - CoverExMinHandle;
            }
            if (MOSAIC_RGN == enType) {
                stChnAttr.unChnAttr.stMosaicChn.stRect.s32X = 400 + 100 * (i - MosaicMinHandle);
                stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = 20 + 100 * (i - MosaicMinHandle);
                stChnAttr.unChnAttr.stMosaicChn.u32Layer = i - MosaicMinHandle;
            }
            if (MOSAICEX_RGN == enType) {
                stChnAttr.unChnAttr.stMosaicChn.stRect.s32X = 800 + 100 * (i - MosaicExMinHandle);
                stChnAttr.unChnAttr.stMosaicChn.stRect.s32Y = 20 + 100 * (i - MosaicExMinHandle);
                stChnAttr.unChnAttr.stMosaicChn.u32Layer = i - MosaicExMinHandle;
            }
            s32Ret = SAMPLE_REGION_AttachToChn(i + j * HandleNum, pstMppChn, &stChnAttr);
            if (TS_SUCCESS != s32Ret) {
                __ERR("SAMPLE_REGION_AttachToChn failed!\n");
                break;
            }
        }
        /*detach region from chn */
        if (TS_SUCCESS != s32Ret && i > 0) {
            tmp_index = i + j * HandleNum;
            for (; tmp_index >= s32MinHandle + j * HandleNum; tmp_index--)
                s32Ret = SAMPLE_REGION_DetachFromChn(tmp_index, pstMppChn);
        }
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_DetachFrmChn(TS_S32 HandleNum, RGN_TYPE_E enType, MPP_CHN_S *pstMppChn)
{
    TS_S32 i;
    TS_S32 s32Ret = TS_SUCCESS;
    TS_S32 s32MinHandle;

    if (HandleNum <= 0 || HandleNum > 16) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    if (TS_NULL == pstMppChn) {
        __ERR("pstMppChn is NULL !\n");
        return TS_FAILURE;
    }
    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        break;
    default:
        break;
    }
    for (i = s32MinHandle; i < s32MinHandle + HandleNum; i++) {
        s32Ret = SAMPLE_REGION_DetachFromChn(i, pstMppChn);
        if (TS_SUCCESS != s32Ret)
            __ERR("SAMPLE_REGION_DetachFromChn failed! Handle:%d\n", i);
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_DetachFrmMultiChn(TS_S32 HandleNum, RGN_TYPE_E enType, MOD_ID_E enModId, TS_S32 s32DevId,
                                            TS_S32 chnCnt)
{
    TS_S32 i;
    TS_S32 j;
    TS_S32 s32Ret = TS_SUCCESS;
    TS_S32 s32MinHandle;
    MPP_CHN_S stChn = {0};
    MPP_CHN_S *pstMppChn = &stChn;

    pstMppChn->enModId = enModId;
    pstMppChn->s32DevId = s32DevId;
    pstMppChn->s32ChnId = 0;

    if (HandleNum <= 0 || HandleNum > 16) {
        __ERR("HandleNum is illegal %d!\n", HandleNum);
        return TS_FAILURE;
    }
    if (enType < 0 || enType > 5) {
        __ERR("enType is illegal %d!\n", enType);
        return TS_FAILURE;
    }
    if (TS_NULL == pstMppChn) {
        __ERR("pstMppChn is NULL !\n");
        return TS_FAILURE;
    }
    switch (enType) {
    case OVERLAY_RGN:
        s32MinHandle = OverlayMinHandle;
        break;
    case OVERLAYEX_RGN:
        s32MinHandle = OverlayExMinHandle;
        break;
    case COVER_RGN:
        s32MinHandle = CoverMinHandle;
        break;
    case COVEREX_RGN:
        s32MinHandle = CoverExMinHandle;
        break;
    case MOSAIC_RGN:
        s32MinHandle = MosaicMinHandle;
        break;
    case MOSAICEX_RGN:
        s32MinHandle = MosaicExMinHandle;
        break;
    default:
        break;
    }
    for (j = 0; j < chnCnt; j++) {
        pstMppChn->s32ChnId = j;
        for (i = 0; i < HandleNum; i++) {
            s32Ret = SAMPLE_REGION_DetachFromChn(s32MinHandle + i + j * HandleNum, pstMppChn);
            if (TS_SUCCESS != s32Ret)
                __ERR("SAMPLE_REGION_DetachFromChn failed! Handle:%d\n", s32MinHandle + i + j * HandleNum);
        }
    }
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_SetBitMap(RGN_HANDLE Handle, PIXEL_FORMAT_E enPixelFmt)
{
    TS_S32 s32Ret;
    BITMAP_S stBitmap;

    REGION_MST_LoadBmp(g_pathBMP, &stBitmap, TS_FALSE, 0, enPixelFmt);
    s32Ret = SAMPLE_REGION_SetBitMap(Handle, &stBitmap);
    if (s32Ret != TS_SUCCESS)
        __ERR("SAMPLE_REGION_SetBitMap failed!Handle:%d\n", Handle);

    free(stBitmap.pData);
    return s32Ret;
}

TS_S32 SAMPLE_COMM_REGION_GetUpCanvas(RGN_HANDLE Handle)
{
    TS_S32 s32Ret;
    SIZE_S stSize;
    BITMAP_S stBitmap;
    RGN_CANVAS_INFO_S stCanvasInfo;

    s32Ret = TS_MPI_RGN_GetCanvasInfo(Handle, &stCanvasInfo);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_GetCanvasInfo failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }

    stBitmap.pData = (TS_VOID *)(TS_UL)stCanvasInfo.u64VirtAddr;
    //stBitmap.pData   = (TS_VOID *)(TS_UL)stCanvasInfo.p32VirtAddr;
    stSize.u32Width = stCanvasInfo.stSize.u32Width;
    stSize.u32Height = stCanvasInfo.stSize.u32Height;
    REGION_MST_UpdateCanvas(g_pathBMP, &stBitmap, TS_FALSE, 0, &stSize, stCanvasInfo.u32Stride, PIXEL_FORMAT_ARGB_1555);

    s32Ret = TS_MPI_RGN_UpdateCanvas(Handle);
    if (TS_SUCCESS != s32Ret) {
        __ERR("TS_MPI_RGN_UpdateCanvas failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    return s32Ret;
}

#define LOG_D(fmt, ...)                                                                                                \
    do {                                                                                                               \
        if (0)                                                                                                         \
            printf("%s:%d:" fmt, __func__, __LINE__, ##__VA_ARGS__);                                                   \
    } while (0);

//region
#define FF_OSD_LINEWIDTH (18)
#define FF_REGION_W (FF_OSD_LINEWIDTH * 6)
#define FF_REGION_H (FF_OSD_LINEWIDTH * 3)
#define OSD_THREAD_PARAM_NUM (8 * 3 + 1)

#define MAX_OSD_CHN_NUM (3)
#define MAX_OSD_TIME_CHN_NUM 4
#define REGION_NUM_PER_CHN (3)

typedef struct {
    TS_S32 u32Width;
    TS_S32 u32Height;
    PIXEL_FORMAT_E enFormat;
    TS_CHAR *pAddr_1; //Y, YUV
    TS_CHAR *pAddr_2; //U, V, UV, VU
    TS_CHAR *pAddr_3; //V, U
    TS_S32 s32Len_1;  //Y, YUV
    TS_S32 s32Len_2;  //U, V, UV, VU
    TS_S32 s32Len_3;  //V, U
} FRAME_S;

typedef struct {
    RGN_HANDLE hdl[OSD_THREAD_PARAM_NUM];
    BITMAP_S stBitmap1;
    BITMAP_S stBitmap2;
    TS_S32 s32ChannelID;
    TS_U32 s32Quit;
} OSD_THREAD_PARAM_t;

typedef struct {
    RGN_HANDLE hdl;
    TS_S32 s32DevId;
    TS_S32 s32ChannelID;
    PIXEL_FORMAT_E enPixelFormat;
} OSD_THREAD_TIME_PARAM_t;
static pthread_t t_osdUpdate[MAX_OSD_CHN_NUM];

static pthread_t t_tidBak;

static OSD_THREAD_PARAM_t g_stThreadParamOsdUpdate[MAX_OSD_CHN_NUM] = {0};

static RGN_HANDLE g_ffHdl = 0;
static TS_U32 g_osdTimeWorkFlag = 0;
static pthread_mutex_t g_osdMutexLock;
static TS_U32 g_sampleVencTerminalAll = 0;
static pthread_t t_osdUpdateTimeStamp[MAX_OSD_TIME_CHN_NUM][MAX_OSD_TIME_CHN_NUM];
static OSD_THREAD_TIME_PARAM_t g_stThreadParamOsdUpdateTime[MAX_OSD_TIME_CHN_NUM][MAX_OSD_TIME_CHN_NUM] = {0};

static TS_S32 g_runFlag = 1;

static RGN_HANDLE g_hdlArray[OSD_THREAD_PARAM_NUM] = {-1};
static BITMAP_S s_bitmap = {
    .enPixelFormat = PIXEL_FORMAT_ARGB_8888,
    .u32Width = FF_REGION_W,
    .u32Height = FF_REGION_H,
    .pData = NULL,
};

TS_S32 SAMPLE_COMM_DEBUG_OPEN_REGION(TS_S32 HandleNum, RGN_TYPE_E enType, MPP_CHN_S *pstChn)
{
    TS_S32 i;
    TS_S32 s32Ret;
    TS_S32 s32MinHandle;

    s32Ret = SAMPLE_COMM_REGION_Create(HandleNum, enType);
    if (TS_SUCCESS != s32Ret) {
        __ERR("SAMPLE_COMM_REGION_Create failed!\n");
        goto EXIT1;
    }

    s32Ret = SAMPLE_COMM_REGION_AttachToChn(HandleNum, enType, pstChn);
    if (TS_SUCCESS != s32Ret) {
        __ERR("SAMPLE_COMM_REGION_AttachToChn failed!\n");
        goto EXIT2;
    }

    s32MinHandle = SAMPLE_COMM_REGION_GetMinHandle(enType);
    __ERR("SAMPLE_COMM_REGION_SetBitMap s32MinHandle=%d, HandleNum=%d\n", s32MinHandle, HandleNum);
    if (OVERLAY_RGN == enType || OVERLAYEX_RGN == enType) {
        for (i = s32MinHandle; i < s32MinHandle + HandleNum; i++) {
            s32Ret = SAMPLE_COMM_REGION_SetBitMap(i, PIXEL_FORMAT_ARGB_8888);
            if (TS_SUCCESS != s32Ret) {
                __ERR("SAMPLE_COMM_REGION_SetBitMap failed!\n");
                goto EXIT2;
            }
        }
    }
    //PAUSE();

    return TS_SUCCESS;

EXIT2:
    s32Ret = SAMPLE_COMM_REGION_DetachFrmChn(HandleNum, enType, pstChn);
    if (TS_SUCCESS != s32Ret)
        __ERR("SAMPLE_COMM_REGION_AttachToChn failed!\n");
EXIT1:
    s32Ret = SAMPLE_COMM_REGION_Destroy(HandleNum, enType);
    if (TS_SUCCESS != s32Ret)
        __ERR("SAMPLE_COMM_REGION_AttachToChn failed!\n");

    //SAMPLE_REGION_MPP_VI_VPSS_VENC_END();
    return s32Ret;
}

TS_S32 SAMPLE_COMM_DEBUG_CLOSE_REGION(TS_S32 HandleNum, RGN_TYPE_E enType, MPP_CHN_S *pstChn)
{
    TS_S32 s32Ret;

    __ERR("SAMPLE_COMM_REGION_DetachFrmChn 1\n");
    s32Ret = SAMPLE_COMM_REGION_DetachFrmChn(HandleNum, enType, pstChn);
    if (TS_SUCCESS != s32Ret)
        __ERR("SAMPLE_COMM_REGION_AttachToChn failed!\n");

    s32Ret = SAMPLE_COMM_REGION_Destroy(HandleNum, enType);
    if (TS_SUCCESS != s32Ret)
        __ERR("SAMPLE_COMM_REGION_AttachToChn failed!\n");

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Frame2MemSize(FRAME_S *stFrame)
{
    if (!stFrame) {
        __ERR("NULL!\n");
        return TS_FAILURE;
    }

    switch (stFrame->enFormat) {
    case PIXEL_FORMAT_NV_12:
    case PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        return stFrame->u32Width * stFrame->u32Height * 3 / 2;
    case PIXEL_FORMAT_ARGB_8888:
        return stFrame->u32Width * stFrame->u32Height * 4;
    case PIXEL_FORMAT_ARGB_4444:
        return stFrame->u32Width * stFrame->u32Height * 2;
    default:
        __ERR("Unknown foramt %d!\n", stFrame->enFormat);
        return 0;
    }
}

TS_S32 SAMPLE_COMM_REGION_Alloc_Paint_Bitmap(MPP_CHN_S *pstMppChn, BITMAP_S *pstBitmap, TS_CHAR R, TS_CHAR G, TS_CHAR B,
                                             TS_CHAR A)
{
    TS_S32 s32MemSize = 0;
    TS_S32 i, j;
    TS_S32 s32Stride = 0;
    FILE *pFile;
    TS_CHAR cOsdName[128] = {0};
    size_t writen;
    TS_CHAR *cTmpPixel = NULL;

    if (!pstBitmap) {
        __ERR("NULL!\n");
        return TS_FAILURE;
    }

    switch (pstBitmap->enPixelFormat) {
    case PIXEL_FORMAT_RGB_888:
        //		s32Stride = pstBitmap->u32Width * 3;
        //		s32MemSize = pstBitmap->u32Height * s32Stride;
        //		pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        //		if (!pstBitmap->pData) {
        //			__ERR("malloc for pstBitmap failed!\n");
        //			return TS_FAILURE;
        //		}
        //		for (i = 0; i < pstBitmap->u32Height; ++i) {
        //			row_start = pstBitmap->pData + i * s32Stride;
        //			memset(row_start, gray_level, s32Stride);
        //			gray_level++;
        //		}
        break;
    case PIXEL_FORMAT_ARGB_8888:
        s32Stride = pstBitmap->u32Width * 4;
        s32MemSize = pstBitmap->u32Height * s32Stride;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            __ERR("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }
        for (i = 0; i < pstBitmap->u32Height; ++i) {
            for (j = 0; j < pstBitmap->u32Width; ++j) {
                cTmpPixel = (pstBitmap->pData + i * s32Stride) + j * 4;
                *cTmpPixel = B;
                *(cTmpPixel + 1) = G;
                *(cTmpPixel + 2) = R;
                *(cTmpPixel + 3) = A;
            }
        }

        LOG_D("pstBitmap %p\n", pstBitmap);
        LOG_D("pstBitmap->pData %p\n", pstBitmap->pData);

        do {
            sprintf(cOsdName, "bulit_in_osd_chn%d_%dx%d_0x%02x%02x%02x%02x.argb8888", pstMppChn->s32DevId,
                    pstBitmap->u32Width, pstBitmap->u32Height, A, R, G, B);
            pFile = fopen(cOsdName, "wb");
            if (!pFile) {
                __ERR("open file [%s] faild\n", cOsdName);
                return TS_FAILURE;
            }
            writen = fwrite(pstBitmap->pData, 1, s32MemSize, pFile);
            if (writen != s32MemSize) {
                __ERR("writen %zd != s32MemSize %d\n", writen, s32MemSize);
            }
            fclose(pFile);
        } while (0);

        break;
    case PIXEL_FORMAT_ARGB_2BIT:
        s32Stride = pstBitmap->u32Width / 4;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width / 4;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }
        memset(pstBitmap->pData, 0x00, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4, 0x55, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4, 0xAA, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4 + s32MemSize / 4, 0xff, s32MemSize / 4);
        break;
    case PIXEL_FORMAT_ARGB_4BIT:
        s32Stride = pstBitmap->u32Width / 2;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width / 2;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }

        memset(pstBitmap->pData, 0x00, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4, 0x55, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4, 0xAA, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4 + s32MemSize / 4, 0xff, s32MemSize / 4);
        break;
    case PIXEL_FORMAT_ARGB_1BIT:
        s32Stride = pstBitmap->u32Width / 8;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width / 8;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }

        memset(pstBitmap->pData, 0xff, s32MemSize);
        memset(pstBitmap->pData + s32MemSize / 4, 0x55, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4, 0xAA, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4 + s32MemSize / 4, 0xff, s32MemSize / 4);
        break;
    case PIXEL_FORMAT_ARGB_1555:
        s32Stride = pstBitmap->u32Width * 2;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width * 2;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }

        memset(pstBitmap->pData, 0x00, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4, 0x55, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4, 0xAA, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4 + s32MemSize / 4, 0xff, s32MemSize / 4);
        break;
    case PIXEL_FORMAT_ARGB_4444:
        s32Stride = pstBitmap->u32Width * 2;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width * 2;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }
        for (i = 0; i < pstBitmap->u32Height; ++i) {
            for (j = 0; j < pstBitmap->u32Width; ++j) {
                cTmpPixel = (pstBitmap->pData + i * s32Stride) + j * 2;
                *cTmpPixel = (G & 0xf0) | (B & 0xf);
                *(cTmpPixel + 1) = (A & 0xf0) | (R & 0xf);
            }
        }

        break;
    case PIXEL_FORMAT_RGB_565:
        s32Stride = pstBitmap->u32Width * 2;
        s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width * 2;
        pstBitmap->pData = (TS_CHAR *)malloc(s32MemSize);
        if (!pstBitmap->pData) {
            printf("malloc for pstBitmap failed!\n");
            return TS_FAILURE;
        }

        memset(pstBitmap->pData, 0x00, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4, 0x55, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4, 0xAA, s32MemSize / 4);
        memset(pstBitmap->pData + s32MemSize / 4 + s32MemSize / 4 + s32MemSize / 4, 0xff, s32MemSize / 4);
        break;
    default:
        __ERR("Unknown format %d!\n", pstBitmap->enPixelFormat);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

void SAMPLE_COMM_REGION_Osd_Proc_Stop(void)
{
    g_runFlag = 0;

    __ERR("BF pthread_join\n");

    pthread_join(t_tidBak, TS_NULL);

    __ERR("AF pthread_join\n");
}

static void *SAMPLE_COMM_REGION_Osd_Refresh_Time_Proc(void *args)
{
    OSD_THREAD_TIME_PARAM_t *pParam = (OSD_THREAD_TIME_PARAM_t *)args;
    if (!pParam) {
        __ERR("NULL!\n");
        return NULL;
    }

    TS_CHAR cName[64];
    snprintf(cName, sizeof(cName), "Osd_Update_time_Proc_%d", pParam->s32ChannelID);
    prctl(PR_SET_NAME, cName);

    TS_U8 u8Length = 0;
    RGN_HANDLE handle = pParam->hdl;
    TS_U8 u8GetTimeInfo[128] = {0};
    BITMAP_S stBitMap = {0};
    TS_U8 *pStream = TS_NULL;
    TS_S32 s32Ret = TS_FAILURE;
    TS_S32 s32Width, s32Height;
    TS_FLOAT pixels;
    static TS_BOOL bChange = TS_FALSE;
    TS_S32 s32BitmapH;
    VENC_CHN_ATTR_S stVencChnAttr = {0};
    while (!g_sampleVencTerminalAll) {
        memset(u8GetTimeInfo, 0, sizeof(u8GetTimeInfo));
        if (0 != osdGetCurrentTime(u8GetTimeInfo, sizeof(u8GetTimeInfo), &u8Length)) {
            usleep(1000 * 1000 * 3);
            continue;
        }
        if (g_osdTimeWorkFlag) {
            RGN_CANVAS_INFO_S stCanvasInfo = {0};
            TS_MPI_VENC_GetChnAttr(pParam->s32ChannelID, &stVencChnAttr);

            s32Ret = osdGetSuitableFontSize(stVencChnAttr.stVencAttr.u32MaxPicWidth,
                                            stVencChnAttr.stVencAttr.u32MaxPicHeight, &pixels, &s32BitmapH);
            //printf("##pixels=%f, s32OneWordH=%d\n", pixels, s32OneWordH);
            if (s32Ret == 0) {
                printf("osdGetSuitableFontSize failed ! s32Width or s32Height invalid!\n");
                continue;
            }
            bChange = TS_FALSE;
            pthread_mutex_lock(&g_osdMutexLock);
            if (pParam->enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
                pStream =
                    getArgbDataAfterHandleStr(u8GetTimeInfo, &s32Width, &s32Height, pixels, s32BitmapH, NULL, bChange);
            } else if (pParam->enPixelFormat == PIXEL_FORMAT_ARGB_2BIT) {
                pStream =
                    getArgbDataAfterHandleStr_2bit(u8GetTimeInfo, &s32Width, &s32Height, pixels, s32BitmapH, NULL);
            }

            if (NULL == pStream) {
                printf(" PDT_MEDIA_RgnLoadIniInfo Error !.");
                pthread_mutex_unlock(&g_osdMutexLock);
                continue;
            }
            stBitMap.pData = pStream;
            stBitMap.enPixelFormat = pParam->enPixelFormat;
            stBitMap.u32Width = s32Width;
            stBitMap.u32Height = s32Height;

            TS_MPI_RGN_GetCanvasInfo(handle, &stCanvasInfo);
            if (pParam->enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
                memcpy((TS_VOID *)(TS_UL)(stCanvasInfo.u64VirtAddr), stBitMap.pData,
                       stBitMap.u32Width * stBitMap.u32Height * 2);
            } else if (pParam->enPixelFormat == PIXEL_FORMAT_ARGB_2BIT) {
                memcpy((TS_VOID *)(TS_UL)(stCanvasInfo.u64VirtAddr), stBitMap.pData,
                       stBitMap.u32Width * stBitMap.u32Height / 4);
            } else {
                printf(" this is not  support !\n");
            }
            free(stBitMap.pData);
            pStream = NULL;

            TS_MPI_RGN_UpdateCanvas(handle);

            if (NULL == stBitMap.pData) {
                printf("stBitMap.pData is free!\n");
            }

            pthread_mutex_unlock(&g_osdMutexLock);
        }
        usleep(990 * 1000);
    }
    return NULL;
}

TS_VOID *SAMPLE_COMM_REGION_Osd_Update_Proc(TS_VOID *param)
{
    OSD_THREAD_PARAM_t *pParam = (OSD_THREAD_PARAM_t *)param;
    if (!pParam) {
        __ERR("NULL!\n");
        return NULL;
    }

    TS_CHAR cName[64];
    snprintf(cName, sizeof(cName), "Osd_Update_Proc_%d", pParam->s32ChannelID);
    prctl(PR_SET_NAME, cName);

    TS_S32 i;
    TS_S32 s32Ret;
    RGN_HANDLE tmpHdl = -1;
    RGN_CANVAS_INFO_S stCanvasInfo;
    BITMAP_S *stBitmap1 = &pParam->stBitmap1;
    BITMAP_S *stBitmap2 = &pParam->stBitmap2;
    BITMAP_S *pTmpBitmap = stBitmap1;
    //void *tmp_ptr = NULL;
    TS_S32 s32MemSize = 0;
    FRAME_S stFrame = {0};
    TS_S32 ts32LoopCnt = 0;

    for (i = 0; i < OSD_THREAD_PARAM_NUM; ++i) {
        LOG_D("hdlArray:\n");
        LOG_D("%d ", pParam->hdl[i]);
        LOG_D("\n");
    }

    LOG_D("stBitmap1 %p\n", stBitmap1);
    LOG_D("stBitmap1->pData %p\n", stBitmap1->pData);
    LOG_D("stBitmap2 %p\n", stBitmap2);
    LOG_D("stBitmap2->pData %p\n", stBitmap2->pData);

    while (!pParam->s32Quit) {
        sleep(5);

        if (!g_runFlag)
            break;

        __ERR("update channel_%d, osd, loop count is %d\n", pParam->s32ChannelID, ts32LoopCnt);
        if ((ts32LoopCnt) % 2) {
            pTmpBitmap = stBitmap1;
        } else {
            pTmpBitmap = stBitmap2;
        }
        ts32LoopCnt++;

        for (i = 0; i < REGION_NUM_PER_CHN; ++i) {
            if (-1 == pParam->hdl[i]) {
                break;
            }
            tmpHdl = pParam->hdl[i];

            //LOG_D("tmpHdl %d\n", tmpHdl);

            s32Ret = TS_MPI_RGN_GetCanvasInfo(tmpHdl, &stCanvasInfo);
            if (TS_SUCCESS != s32Ret) {
                __ERR("TS_MPI_RGN_GetCanvasInfo failed with %#x!\n", s32Ret);
                break;
            }

            stFrame.u32Width = stCanvasInfo.stSize.u32Width;
            stFrame.u32Height = stCanvasInfo.stSize.u32Height;
            stFrame.enFormat = stCanvasInfo.enPixelFmt;
            //printf("stFrame.u32Width=%d\n",stFrame.u32Width);
            //printf("stFrame.u32Height=%d\n",stFrame.u32Height);
            //LOG_D("VirtAddr %p, W %d, H%d, format %d\n", (TS_VOID*)(TS_UL)(stCanvasInfo.u64VirtAddr),
            //	stFrame.u32Width, stFrame.u32Height, stFrame.format);
            //LOG_D("pData %p\n", pTmpBitmap->pData);
            s32MemSize = SAMPLE_COMM_REGION_Frame2MemSize(&stFrame);
            memcpy((TS_VOID *)(TS_UL)(stCanvasInfo.u64VirtAddr), pTmpBitmap->pData, s32MemSize);
            //LOG_D("memcpy end, VirtAddr %lld, pData %p, u32CanvMemSize %d, tmp %p\n", stCanvasInfo.u64VirtAddr,
            //	pTmpBitmap->pData, s32MemSize, tmp_ptr);

            s32Ret = TS_MPI_RGN_UpdateCanvas(tmpHdl);
            if (TS_SUCCESS != s32Ret) {
                __ERR("TS_MPI_RGN_UpdateCanvas failed with %#x!\n", s32Ret);
                break;
            }
        }
    }

    return NULL;
}

TS_S32 SAMPLE_COMM_REGION_Osd_Update_Init(RGN_HANDLE hdl[], MPP_CHN_S *pstMppChn, BITMAP_S *stBitmap1,
                                          BITMAP_S *stBitmap2)
{
    TS_S32 s32Ret;
    TS_S32 ts32PipeId = 0;
    if (!stBitmap1 || !stBitmap2) {
        __ERR("NULL!\n");
        return TS_FAILURE;
    }
    if (pstMppChn->enModId == TS_ID_VPSS) {
        ts32PipeId = pstMppChn->s32DevId;
    } else {
        ts32PipeId = pstMppChn->s32ChnId;
    }

    if (ts32PipeId >= MAX_OSD_CHN_NUM || ts32PipeId < 0) {
        __ERR("ts32PipeId err is %d\n", ts32PipeId);
        return TS_FAILURE;
    }

    memcpy(g_stThreadParamOsdUpdate[ts32PipeId].hdl, hdl, sizeof(g_stThreadParamOsdUpdate[ts32PipeId].hdl));

    g_stThreadParamOsdUpdate[ts32PipeId].stBitmap1 = *stBitmap1;
    g_stThreadParamOsdUpdate[ts32PipeId].stBitmap2 = *stBitmap2;
    g_stThreadParamOsdUpdate[ts32PipeId].s32ChannelID = ts32PipeId;
    g_stThreadParamOsdUpdate[ts32PipeId].s32Quit = 0;
    //	memcpy(&g_stThreadParamOsdUpdate.stBitmap1, stBitmap1, sizeof(g_stThreadParamOsdUpdate.stBitmap1));
    //	memcpy(&g_stThreadParamOsdUpdate.stBitmap2, stBitmap2, sizeof(g_stThreadParamOsdUpdate.stBitmap2));

    s32Ret = pthread_create(&t_osdUpdate[ts32PipeId], 0, SAMPLE_COMM_REGION_Osd_Update_Proc,
                            (void *)&g_stThreadParamOsdUpdate[ts32PipeId]);
    if (s32Ret != TS_SUCCESS) {
        __ERR("pthread_create osd_update_worker failed !\n");
        return TS_FAILURE;
    }

    t_tidBak = t_osdUpdate[ts32PipeId];

    return TS_SUCCESS;
}
TS_S32 SAMPLE_COMM_REGION_Dettach_Dynamic_Rgn(MPP_CHN_S *pstMppChn)
{
    TS_S32 s32Ret;
    TS_S32 i;
    TS_S32 ts32PipeId = 0;

    if (NULL == pstMppChn) {
        __ERR("SAMPLE_COMM_REGION_Dettach_Dynamic_Rgn pstMppChn is NULL!\n");
        return TS_FAILURE;
    }

    if (pstMppChn->enModId == TS_ID_VPSS) {
        ts32PipeId = pstMppChn->s32DevId;
    } else {
        ts32PipeId = pstMppChn->s32ChnId;
    }

    g_stThreadParamOsdUpdate[ts32PipeId].s32Quit = 1;
    if (t_osdUpdate[ts32PipeId] >= 0) {
        pthread_join(t_osdUpdate[ts32PipeId], 0);
    }

    for (i = 0; i < REGION_NUM_PER_CHN; ++i) {
        s32Ret = TS_MPI_RGN_DetachFromChn(g_stThreadParamOsdUpdate[ts32PipeId].hdl[i], pstMppChn);
        if (TS_SUCCESS != s32Ret) {
            __ERR("APP_REGION_DetachFromChn failed! Handle:%d\n", i);
        }
        s32Ret = TS_MPI_RGN_Destroy(g_stThreadParamOsdUpdate[ts32PipeId].hdl[i]);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Destroy failed! Handle:%d\n", i);
        }
    }
    if (ts32PipeId == 0)
        g_ffHdlExt[ts32PipeId] = 0;
    else
        g_ffHdlExt[ts32PipeId] = 3;

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Dettach_Dynamic_OverlayRgn(MPP_CHN_S *pstMppChn)
{
    TS_S32 s32Ret;
    TS_S32 i;

    if (NULL == pstMppChn) {
        __ERR("SAMPLE_COMM_REGION_Dettach_Dynamic_Rgn pstMppChn is NULL!\n");
        return TS_FAILURE;
    }

    for (i = 0; i < g_ffHdl; ++i) {

        s32Ret = TS_MPI_RGN_DetachFromChn(i, pstMppChn);
        if (TS_SUCCESS != s32Ret) {
            __ERR("APP_REGION_DetachFromChn failed! Handle:%d\n", i);
        }
        s32Ret = TS_MPI_RGN_Destroy(i);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Destroy failed! Handle:%d\n", i);
        }
    }
    g_ffHdl = 0;

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Attach_Dynamic_Rgn(MPP_CHN_S *pstMppChn, TS_BOOL ex)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_HANDLE hdlArray[OSD_THREAD_PARAM_NUM] = {-1};
    for (i = 0; i < OSD_THREAD_PARAM_NUM; i++) {
        hdlArray[i] = -1;
    }
    TS_S32 ts32HdlArrayIdx = 0;
    TS_S32 ts32MultipleX = 0;
    TS_S32 ts32MultipleY = 0;

    PIXEL_FORMAT_E enType = PIXEL_FORMAT_ARGB_8888;
    enType = PIXEL_FORMAT_ARGB_4444;

    RGN_ATTR_S stRegion = {
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = enType,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = FF_REGION_W,
                                .u32Height = FF_REGION_H,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };
    BITMAP_S stBitmap = {
        .enPixelFormat = enType,
        .u32Width = FF_REGION_W,
        .u32Height = FF_REGION_H,
        .pData = NULL,
    };
    BITMAP_S stBitmap2 = {
        .enPixelFormat = enType,
        .u32Width = FF_REGION_W,
        .u32Height = FF_REGION_H,
        .pData = NULL,
    };
    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = true,
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = 0,
                        .u16ColorLUT = {},
                    },
            },
    };

    if (NULL == stBitmap.pData) {
        s32Ret = SAMPLE_COMM_REGION_Alloc_Paint_Bitmap(pstMppChn, &stBitmap, 0xff, 0, 0, 0xff);
        if (s32Ret != TS_SUCCESS) {
            __ERR("SAMPLE_COMM_REGION_Alloc_Paint_Bitmap failed %d!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    if (NULL == stBitmap2.pData) {
        s32Ret = SAMPLE_COMM_REGION_Alloc_Paint_Bitmap(pstMppChn, &stBitmap2, 0, 0xff, 0, 0xff);
        if (s32Ret != TS_SUCCESS) {
            __ERR("SAMPLE_COMM_REGION_Alloc_Paint_Bitmap failed %d!\n", s32Ret);
            return TS_FAILURE;
        }
    }

    if (pstMppChn->enModId == TS_ID_VPSS) {
#ifdef OSD_RESTRUCT
        s32Ret = TS_MPI_RGN_BatchBegin(pstMppChn->s32DevId, pstMppChn->s32ChnId);
        if (s32Ret != TS_SUCCESS) {
            __ERR("TS_MPI_RGN_BatchBegin failed %d!\n", s32Ret);
            return TS_FAILURE;
        }
#endif
        VPSS_CHN_ATTR_S pstChnAttr = {0};
        TS_MPI_VPSS_GetChnAttr(pstMppChn->s32DevId, pstMppChn->s32ChnId, &pstChnAttr);

        ts32MultipleX = pstChnAttr.u32Width / FF_REGION_W;
        ts32MultipleY = pstChnAttr.u32Height / FF_REGION_H;

        for (i = 0; i < REGION_NUM_PER_CHN; ++i) {

            if (i == 1) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = (ts32MultipleX - 1) * FF_REGION_W;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = (ts32MultipleY - 1) * FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            } else if (i == 0) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = FF_REGION_W + FF_OSD_LINEWIDTH;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            } else if (i == 2) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = (ts32MultipleX - 1) * FF_REGION_W;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            }

            s32Ret = TS_MPI_RGN_Create(g_ffHdlExt[pstMppChn->s32DevId], &stRegion);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdlExt[pstMppChn->s32DevId], &stBitmap);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_SetBitMap failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdlExt[pstMppChn->s32DevId], pstMppChn, &stChnAttr);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_AttachToChn failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            hdlArray[ts32HdlArrayIdx++] = g_ffHdlExt[pstMppChn->s32DevId];
            g_ffHdlExt[pstMppChn->s32DevId]++;
        }

#ifdef OSD_RESTRUCT
        s32Ret = TS_MPI_RGN_BatchEnd(pstMppChn->s32DevId, pstMppChn->s32ChnId);
        if (s32Ret != TS_SUCCESS) {
            __ERR("TS_MPI_RGN_BatchEnd failed %d!\n", s32Ret);
            return TS_FAILURE;
        }
#endif
    } else {
        VPSS_CHN_ATTR_S pstChnAttr = {0};
        TS_MPI_VPSS_GetChnAttr(0, 0, &pstChnAttr);
        VENC_CHN_ATTR_S stVencChnAttr = {0};
        TS_MPI_VENC_GetChnAttr(pstMppChn->s32ChnId, &stVencChnAttr);
        ts32MultipleX = stVencChnAttr.stVencAttr.u32PicWidth / FF_REGION_W;
        ts32MultipleY = stVencChnAttr.stVencAttr.u32PicHeight / FF_REGION_H;
        if (pstChnAttr.stFastFlow.enFastFlow == FAST_FLOW_MODE_STITCH) {
            ts32MultipleY = ts32MultipleY / 2;
        }
        for (i = 0; i < REGION_NUM_PER_CHN; ++i) {

            if (i == 1) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = (ts32MultipleX - 1) * FF_REGION_W;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = (ts32MultipleY - 1) * FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            } else if (i == 0) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = FF_REGION_W + FF_OSD_LINEWIDTH;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            } else if (i == 2) {
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = (ts32MultipleX - 1) * FF_REGION_W;
                stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = FF_REGION_H;
                //printf("[s32X = %d,s32Y = %d]\n",stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);
            }

            s32Ret = TS_MPI_RGN_Create(g_ffHdlExt[pstMppChn->s32ChnId], &stRegion);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdlExt[pstMppChn->s32ChnId], &stBitmap);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_SetBitMap failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdlExt[pstMppChn->s32ChnId], pstMppChn, &stChnAttr);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_AttachToChn failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            hdlArray[ts32HdlArrayIdx++] = g_ffHdlExt[pstMppChn->s32ChnId];
            g_ffHdlExt[pstMppChn->s32ChnId]++;
        }
    }
    for (i = 0; i < OSD_THREAD_PARAM_NUM; ++i) {
        LOG_D("hdlArray:\n");
        LOG_D("%d \n", hdlArray[i]);
        LOG_D("\n");
    }

    s32Ret = SAMPLE_COMM_REGION_Osd_Update_Init(hdlArray, pstMppChn, &stBitmap, &stBitmap2);
    if (s32Ret != TS_SUCCESS) {
        __ERR("osd_update_init failed %d!\n", s32Ret);
        return TS_FAILURE;
    }

    memcpy(g_hdlArray, hdlArray, sizeof(hdlArray));

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Attach_Dynamic_OverlayRgn(MPP_CHN_S *pstMppChn, POINT_S *pstPoint, SIZE_S *pstSize,
                                                    TS_S32 s32Format)
{
    TS_S32 s32Ret;
    TS_S32 index = 0;
    RGN_ATTR_S stRegion = {
        .enType = OVERLAY_RGN,
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = s32Format,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = pstSize->u32Width,
                                .u32Height = pstSize->u32Height,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };
    BITMAP_S stBitmap = {
        .enPixelFormat = s32Format,
        .u32Width = pstSize->u32Width,
        .u32Height = pstSize->u32Height,
        .pData = NULL,
    };

    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = true,
        .enType = OVERLAY_RGN,
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = 0,
                        .u16ColorLUT = {},
                    },
            },
    };

    //1bit lut
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueB = 0;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueB = 0xff;

    //2bit lut
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueB = 0;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueB = 0xff;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueB = 0xff;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueB = 0xff;

    //4bit lut
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueB = 0;

    for (index = 1; index < 16; index++) {
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0xff;
    }
    if (NULL == stBitmap.pData) {
        s32Ret = SAMPLE_COMM_REGION_Alloc_Paint_Bitmap(pstMppChn, &stBitmap, 0xff, 0, 0, 0xff);
        if (s32Ret != TS_SUCCESS) {
            __ERR("SAMPLE_COMM_REGION_Alloc_Paint_Bitmap failed %d!\n", s32Ret);
            return TS_FAILURE;
        }
    }

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchBeginEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        __ERR("TS_MPI_RGN_BatchBeginEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif
    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = pstPoint->s32X;
    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = pstPoint->s32Y;
    printf("[s32X = %d,s32Y = %d]\n", stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X,
           stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y);

    s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
    if (s32Ret != TS_SUCCESS) {
        __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, &stBitmap);
    if (s32Ret != TS_SUCCESS) {
        __ERR("TS_MPI_RGN_SetBitMap failed %d!\n", s32Ret);
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
    if (s32Ret != TS_SUCCESS) {
        __ERR("TS_MPI_RGN_AttachToChn failed %d!\n", s32Ret);
        return TS_FAILURE;
    }

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchEndEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        __ERR("TS_MPI_RGN_BatchEndEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif

    s32Ret = TS_MPI_RGN_UpdateCanvas(g_ffHdl);
    if (s32Ret != TS_SUCCESS) {
        printf("TS_MPI_RGN_UpdateCanvas failed %d!\n", s32Ret);
        return -1;
    }
    g_ffHdl++;

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Attach_Dynamic_OverlayWin(TS_U32 WIDTH, TS_U32 HEIGHT, MPP_CHN_S *pstMppChn,
                                                    BITMAP_S *pstBitmap)
{
    TS_S32 s32Ret = TS_SUCCESS;

    TS_S32 indej = 0;
    TS_S32 s32MemSize = pstBitmap->u32Height * pstBitmap->u32Width * 2;

    RGN_ATTR_S stRegion = {
        .enType = OVERLAY_RGN,
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = pstBitmap->enPixelFormat,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = pstBitmap->u32Width,
                                .u32Height = pstBitmap->u32Height,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };

    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = TS_TRUE,
        .enType = OVERLAY_RGN,
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = ATTACH_JPEG_MAIN,
                        .u16ColorLUT = {},
                    },
            },
    };

    if (NULL != pstBitmap->pData) {
        memset(pstBitmap->pData, 0xff, s32MemSize);
    }

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchBeginEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        printf("TS_MPI_RGN_BatchBeginEx failed %d!\n", s32Ret);
        return -1;
    }
#endif

    for (indej = 0; indej < 4; ++indej) {
        g_ffHdl = indej;
        s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_Create failed %d!\n", s32Ret);
            return -1;
        }

        memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAY_RGN;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 64 * indej;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = indej * 64;
        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;

        s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, pstBitmap);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_SetBitMap failed %d!\n", s32Ret);
            return -1;
        }

        s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_AttachToChn failed %d!\n", s32Ret);
            return -1;
        }
        s32Ret = TS_MPI_RGN_UpdateCanvas(g_ffHdl);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_UpdateCanvas failed %d!\n", s32Ret);
            return -1;
        }
    }

    if (NULL != pstBitmap->pData) {
        memset(pstBitmap->pData, 0xaa, s32MemSize);
    }

    for (indej = 4; indej < 8; ++indej) {
        g_ffHdl = indej;
        s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_Create failed %d!\n", s32Ret);
            return -1;
        }

        memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAY_RGN;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = pstBitmap->u32Width * (indej - 4) * 2 + 16;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 320;
        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;

        s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, pstBitmap);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_SetBitMap failed %d!\n", s32Ret);
            return -1;
        }

        s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_AttachToChn failed %d!\n", s32Ret);
            return -1;
        }
        s32Ret = TS_MPI_RGN_UpdateCanvas(g_ffHdl);
        if (s32Ret != TS_SUCCESS) {
            printf("TS_MPI_RGN_UpdateCanvas failed %d!\n", s32Ret);
            return -1;
        }
    }
#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchEndEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        printf("TS_MPI_RGN_BatchEndEx failed %d!\n", s32Ret);
        return -1;
    }
#endif

    return s32Ret;
    printf("Test Region stop and destroy success!");
}

TS_S32 SAMPLE_COMM_REGION_Osd_Update_Time(RGN_HANDLE hdl, MPP_CHN_S *pstMppChn, PIXEL_FORMAT_E enPixelFormat)
{
    TS_S32 s32Ret;

    if (pstMppChn->s32DevId > MAX_OSD_CHN_NUM || pstMppChn->s32DevId < 0) {
        __ERR("s32DevId err is %d\n", pstMppChn->s32DevId);
        return TS_FAILURE;
    }
    g_stThreadParamOsdUpdateTime[pstMppChn->s32DevId][pstMppChn->s32ChnId].hdl = hdl;
    g_stThreadParamOsdUpdateTime[pstMppChn->s32DevId][pstMppChn->s32ChnId].s32DevId = pstMppChn->s32DevId;
    g_stThreadParamOsdUpdateTime[pstMppChn->s32DevId][pstMppChn->s32ChnId].s32ChannelID = pstMppChn->s32ChnId;
    g_stThreadParamOsdUpdateTime[pstMppChn->s32DevId][pstMppChn->s32ChnId].enPixelFormat = enPixelFormat;
    s32Ret = pthread_create(&t_osdUpdateTimeStamp[pstMppChn->s32DevId][pstMppChn->s32ChnId], 0,
                            SAMPLE_COMM_REGION_Osd_Refresh_Time_Proc,
                            (void *)&g_stThreadParamOsdUpdateTime[pstMppChn->s32DevId][pstMppChn->s32ChnId]);
    if (s32Ret != TS_SUCCESS) {
        __ERR("pthread_create osd_update_worker failed !\n");
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}
TS_S32 SAMPLE_COMM_REGION_Osd_Timestamp(MPP_CHN_S *pstMppChn, TS_BOOL ex, PIXEL_FORMAT_E enPixelFormat)
{
    TS_S32 s32Ret;

    pthread_mutex_init(&g_osdMutexLock, NULL);
    g_osdTimeWorkFlag = 1;
    g_sampleVencTerminalAll = 0;
    RGN_ATTR_S stRegion = {
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = enPixelFormat,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = FF_REGION_W,
                                .u32Height = FF_REGION_H,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };
    BITMAP_S stBitmap = {
        .enPixelFormat = enPixelFormat,
        .u32Width = FF_REGION_W,
        .u32Height = FF_REGION_H,
        .pData = NULL,
    };

    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = true,
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = 0,
                        .u16ColorLUT = {},
                    },
            },
    };

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchBeginEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_BatchBeginEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif

    VENC_CHN_ATTR_S stVencChnAttr = {0};
    TS_MPI_VENC_GetChnAttr(pstMppChn->s32ChnId, &stVencChnAttr);

    TS_U8 u8Length = 0;
    TS_U8 u8GetTimeInfo[128] = {0};
    TS_S32 s32NeededYHeight, s32Width, s32Height, s32BitmapW, s32BitmapH, s32OneWordH;
    TS_FLOAT pixels;
    TS_U8 *pStream = TS_NULL;

    s32NeededYHeight = osdGetSuitableFontSize(stVencChnAttr.stVencAttr.u32MaxPicWidth,
                                              stVencChnAttr.stVencAttr.u32MaxPicHeight, &pixels, &s32OneWordH);
    //LOG("##pixels=%f, s32OneWordH=%d\n", pixels, s32OneWordH);
    if (s32NeededYHeight == 0) {
        LOG_D("osdGetSuitableFontSize failed ! s32Width or s32Height invalid!\n");
        return TS_FAILURE;
    }

    if (enPixelFormat == PIXEL_FORMAT_ARGB_2BIT) {
        osdGetTimeStringBitmapSize_2bit(pixels, s32OneWordH, &s32BitmapW, &s32BitmapH);
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
        osdGetTimeStringBitmapSize_1555(pixels, s32OneWordH, &s32BitmapW, &s32BitmapH);
    } else {
        LOG_D("this PixelFormat not support!\n");
        return -1;
    }

    stRegion.unAttr.stOverlay.stSize.u32Width = s32BitmapW;
    stRegion.unAttr.stOverlay.stSize.u32Height = s32BitmapH;
    s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_Create %d failed %d!\n", g_ffHdl, s32Ret);
        return TS_FAILURE;
    }

    /* bitmap */
    osdGetCurrentTime(u8GetTimeInfo, sizeof(u8GetTimeInfo), &u8Length);
    if (enPixelFormat == PIXEL_FORMAT_ARGB_2BIT) {
        pStream =
            getArgbDataAfterHandleStr_2bit((TS_U8 *)&u8GetTimeInfo, &s32Width, &s32Height, pixels, s32BitmapH, TS_NULL);
    } else if (enPixelFormat == PIXEL_FORMAT_ARGB_1555) {
        pStream = getArgbDataAfterHandleStr((TS_U8 *)&u8GetTimeInfo, &s32Width, &s32Height, pixels, s32BitmapH, TS_NULL,
                                            TS_TRUE);
    } else {
        LOG_D("this PixelFormat not support!\n");
        return -2;
    }
    stBitmap.pData = pStream;
    stBitmap.enPixelFormat = enPixelFormat;
    stBitmap.u32Width = s32Width;
    stBitmap.u32Height = s32Height;

    s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, &stBitmap);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_SetBitMap %d failed %d!\n", g_ffHdl, s32Ret);
        return TS_FAILURE;
    }

    free(stBitmap.pData);
    pStream = NULL;

    memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
    stChnAttr.bShow = TS_TRUE;
    stChnAttr.enType = OVERLAY_RGN;
    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 16;
    stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = s32NeededYHeight;
    stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
    stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u8InvertColorTh = 100;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u8InvertColorBige = 0;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u8InvertColorOsdThH = 0;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u8InvertColorOsdThL = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stInvertColor.bInvColEn = 1;

    /* 1bit lut  */
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[0].u8ValueB = 0;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel1Bit[1].u8ValueB = 0xff;

    /* 2bit lut */
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[0].u8ValueB = 0;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[1].u8ValueB = 0xff;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[2].u8ValueB = 0xff;

    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8Alpha = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueR = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueG = 0xff;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel2Bit[3].u8ValueB = 0xff;

    /* 4bit lut */
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8Alpha = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueR = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueG = 0;
    stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[0].u8ValueB = 0;
    TS_S32 index;
    for (index = 1; index < 16; index++) {
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8Alpha = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueR = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueG = 0xff;
        stChnAttr.unChnAttr.stOverlayChn.stColorLUT.stPixel4Bit[index].u8ValueB = 0xff;
    }
    s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_AttachToChn %d failed %d!\n", g_ffHdl, s32Ret);
        return TS_FAILURE;
    }

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchEndEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_BatchEndEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif

    if (enPixelFormat == PIXEL_FORMAT_ARGB_2BIT) {
        VENC_ROI_ATTR_S stRoiAttr = {};

        if ((pstMppChn->s32DevId == 0) && (pstMppChn->s32ChnId == 0)) {
            s32Ret = TS_MPI_VENC_GetRoiAttr(0, 0, &stRoiAttr);
        } else if ((pstMppChn->s32DevId == 1) && (pstMppChn->s32ChnId == 0)) {
            s32Ret = TS_MPI_VENC_GetRoiAttr(1, 0, &stRoiAttr);
        }

        if (TS_SUCCESS != s32Ret) {
            __ERR("Get Roi Attr failed for %#x!", s32Ret);
        }
        stRoiAttr.bAbsQp = TS_FALSE; //TS_TRUE;
        stRoiAttr.bEnable = 1;
        stRoiAttr.s32Qp = 37;
        stRoiAttr.u32Index = 0;
        stRoiAttr.stRect.u32Height = s32BitmapH; /* 水印高 16位对齐 */
        stRoiAttr.stRect.u32Width = s32BitmapW;  /* 水印宽 16位对齐 */
        if ((pstMppChn->s32DevId == 0) && (pstMppChn->s32ChnId == 0)) {
            s32Ret = TS_MPI_VENC_SetRoiAttr(0, &stRoiAttr);
        } else if ((pstMppChn->s32DevId == 1) && (pstMppChn->s32ChnId == 0)) {
            s32Ret = TS_MPI_VENC_SetRoiAttr(1, &stRoiAttr);
        }

        if (TS_SUCCESS != s32Ret) {
            __ERR("Set Roi Attr failed for %#x!", s32Ret);
        }
    }

    s32Ret = SAMPLE_COMM_REGION_Osd_Update_Time(g_ffHdl, pstMppChn, enPixelFormat);
    if (s32Ret != TS_SUCCESS) {
        __ERR("SAMPLE_COMM_REGION_Osd_Update_Time failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
    g_ffHdl++;
    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Dettach_Common_Rgn(MPP_CHN_S *pstMppChn)
{
    TS_S32 s32Ret;
    TS_S32 i;

    if (NULL == pstMppChn) {
        __ERR("SAMPLE_COMM_REGION_Dettach_Common_Rgn pstMppChn is NULL!\n");
        return TS_FAILURE;
    }

    if (g_osdTimeWorkFlag == 1) {
        g_osdTimeWorkFlag = TS_FALSE;
        g_sampleVencTerminalAll = 1;
        pthread_join(t_osdUpdateTimeStamp[pstMppChn->s32DevId][pstMppChn->s32ChnId], 0);
    }

    __ERR("Detach OSD In Chn\n");

    for (i = 0; i < g_ffHdl; ++i) {

        s32Ret = TS_MPI_RGN_DetachFromChn(i, pstMppChn);
        if (TS_SUCCESS != s32Ret) {
            __ERR("APP_REGION_DetachFromChn failed! Handle:%d\n", i);
        }
        s32Ret = TS_MPI_RGN_Destroy(i);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Destroy failed! Handle:%d\n", i);
        }
    }
    g_ffHdl = 0;

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Dettach_Common_Num_Rgn(MPP_CHN_S *pstMppChn, TS_S32 s32OsdBeginIndex, TS_S32 s32OsdNum)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_HANDLE osdHandle = 0;

    if (NULL == pstMppChn) {
        __ERR("SAMPLE_COMM_REGION_Dettach_Common_Rgn pstMppChn is NULL!\n");
        return TS_FAILURE;
    }

    if (g_osdTimeWorkFlag == 1) {
        g_osdTimeWorkFlag = TS_FALSE;
        g_sampleVencTerminalAll = 1;
        pthread_join(t_osdUpdateTimeStamp[pstMppChn->s32DevId][pstMppChn->s32ChnId], 0);
    }

    __ERR("Detach OSD In Chn\n");

    for (i = 0; i < s32OsdNum; ++i) {
        osdHandle = s32OsdBeginIndex + i;
        s32Ret = TS_MPI_RGN_DetachFromChn(osdHandle, pstMppChn);
        if (TS_SUCCESS != s32Ret) {
            __ERR("APP_REGION_DetachFromChn failed! Handle:%d\n", osdHandle);
        }
        s32Ret = TS_MPI_RGN_Destroy(osdHandle);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Destroy failed! Handle:%d\n", osdHandle);
        }
    }
    g_ffHdl = 0;
    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Osd_Chinese(MPP_CHN_S *pstMppChn, TS_BOOL ex, PIXEL_FORMAT_E enPixelFormat, TS_S32 s32OsdNum,
                                      TS_S32 s32WordWidth)
{
    TS_S32 s32Ret;
    RGN_ATTR_S stRegion = {
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = enPixelFormat,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = FF_REGION_W,
                                .u32Height = FF_REGION_H,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };
    BITMAP_S stBitmap = {
        .enPixelFormat = enPixelFormat,
        .u32Width = FF_REGION_W,
        .u32Height = FF_REGION_H,
        .pData = NULL,
    };

    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = true,
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = 0,
                        .u16ColorLUT = {},
                    },
            },
    };

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchBeginEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_BatchBeginEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif

    VENC_CHN_ATTR_S stVencChnAttr = {0};
    TS_MPI_VENC_GetChnAttr(pstMppChn->s32ChnId, &stVencChnAttr);

    TS_U8 u8GetTimeInfo[128] = {"在一个区域设置覆盖区域设置三二个汉字主码流副码流子码流都正常显示"};
    TS_S32 s32NeededYHeight, s32Width, s32Height, s32BitmapW, s32BitmapH, s32OneWordH;
    TS_FLOAT pixels;
    TS_U8 *pStream = TS_NULL;

    s32NeededYHeight = osdGetSuitableFontSize(stVencChnAttr.stVencAttr.u32MaxPicWidth,
                                              stVencChnAttr.stVencAttr.u32MaxPicHeight, &pixels, &s32OneWordH);
    switch (s32WordWidth) {
    case 16:
        pixels = 16;
        s32OneWordH = 16;
        break;
    case 32:
        pixels = 32;
        s32OneWordH = 32;
        break;
    case 64:
        pixels = 64;
        s32OneWordH = 64;
        s32NeededYHeight = 72;
        break;
    default:
        break;
    }
    osdGetCustomStringBitmapSize_1555(pixels, u8GetTimeInfo, s32OneWordH, &s32BitmapW, &s32BitmapH);

    stRegion.unAttr.stOverlay.stSize.u32Width = s32BitmapW;
    stRegion.unAttr.stOverlay.stSize.u32Height = s32BitmapH;
    pStream =
        getArgbDataAfterHandleStr((TS_U8 *)&u8GetTimeInfo, &s32Width, &s32Height, pixels, s32BitmapH, TS_NULL, TS_TRUE);

    stBitmap.pData = pStream;
    stBitmap.enPixelFormat = enPixelFormat;
    stBitmap.u32Width = s32Width;
    stBitmap.u32Height = s32Height;
    if (s32OsdNum != 1 && s32OsdNum != 3 && s32OsdNum != 5)
        s32OsdNum = 1;
    for (TS_S32 i = 0; i < s32OsdNum; ++i) {
        s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
        if (s32Ret != TS_SUCCESS) {
            LOG_D("TS_MPI_RGN_Create %d failed %d!\n", g_ffHdl, s32Ret);
            return TS_FAILURE;
        }

        s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, &stBitmap);
        if (s32Ret != TS_SUCCESS) {
            LOG_D("TS_MPI_RGN_SetBitMap %d failed %d!\n", g_ffHdl, s32Ret);
            return TS_FAILURE;
        }

        memset(&stChnAttr, 0, sizeof(RGN_CHN_ATTR_S));
        stChnAttr.bShow = TS_TRUE;
        stChnAttr.enType = OVERLAY_RGN;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 16;
        stChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = s32NeededYHeight + i * s32NeededYHeight * 2;
        stChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 0;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bQpDisable = TS_FALSE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.bAbsQp = TS_TRUE;
        stChnAttr.unChnAttr.stOverlayChn.stQpInfo.s32Qp = 30;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Height = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.stInvColArea.u32Width = 16;
        stChnAttr.unChnAttr.stOverlayChn.stInvertColor.u32LumThresh = 128;

        s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
        if (s32Ret != TS_SUCCESS) {
            LOG_D("TS_MPI_RGN_AttachToChn %d failed %d!\n", g_ffHdl, s32Ret);
            return TS_FAILURE;
        }

        g_ffHdl++;
    }

    free(stBitmap.pData);
    pStream = NULL;

#ifdef OSD_RESTRUCT
    s32Ret = TS_MPI_RGN_BatchEndEx(pstMppChn);
    if (s32Ret != TS_SUCCESS) {
        LOG_D("TS_MPI_RGN_BatchEndEx failed %d!\n", s32Ret);
        return TS_FAILURE;
    }
#endif
    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Start_Rgn(MPP_CHN_S *pstMppChn, TS_BOOL ex, TS_S32 type)
{
    TS_S32 s32Ret;
    TS_S32 i;
    TS_S32 ts32HdlArrayIdx = 0;

    RGN_ATTR_S stRegion = {
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unAttr =
            {
                .stOverlay =
                    {
                        .enPixelFmt = PIXEL_FORMAT_ARGB_8888,
                        .u32BgColor = 0,
                        .stSize =
                            {
                                .u32Width = FF_REGION_W,
                                .u32Height = FF_REGION_H,
                            },
                        .u32CanvasNum = 2,
                    },
            },
    };

    RGN_CHN_ATTR_S stChnAttr = {
        .bShow = true,
        .enType = (ex ? OVERLAYEX_RGN : OVERLAY_RGN),
        .unChnAttr =
            {
                .stOverlayChn =
                    {
                        .stPoint = {0, 0}, //care
                        .u32FgAlpha = 0,
                        .u32BgAlpha = 0,
                        .u32Layer = 0,
                        .stQpInfo = {},
                        .stInvertColor = {},
                        .enAttachDest = 0,
                        .u16ColorLUT = {},
                    },
            },
    };

    RGN_ATTR_S stRegionCover = {
        .enType = (ex ? COVEREX_RGN : COVER_RGN),
        .unAttr = {},
    };

    RGN_CHN_ATTR_S stChnAttrCover = {
        .bShow = true,
        .enType = (ex ? COVEREX_RGN : COVER_RGN),
        .unChnAttr =
            {
                .stCoverChn =
                    {
                        .enCoverType = AREA_RECT,
                        .stRect = {0, FF_REGION_H + 64, FF_REGION_W, FF_REGION_H}, //care
                        .u32Color = 0,
                        .u32Layer = 0,
                        .enCoordinate = RGN_ABS_COOR,
                    },
            },
    };

    RGN_ATTR_S stRegionMosaic = {
        .enType = (ex ? MOSAICEX_RGN : MOSAIC_RGN),
        .unAttr = {},
    };

    RGN_CHN_ATTR_S stChnAttrMosaic = {
        .bShow = true,
        .enType = (ex ? MOSAICEX_RGN : MOSAIC_RGN),
        .unChnAttr =
            {
                .stMosaicChn =
                    {
                        .stRect = {0, (FF_REGION_H + 64) * 2, FF_REGION_W, FF_REGION_H}, //care
                        .enBlkSize = MOSAIC_BLK_SIZE_64,
                        .u32Layer = 0,
                    },
            },
    };

    if (type == 0) {
        s32Ret = SAMPLE_COMM_REGION_Alloc_Paint_Bitmap(pstMppChn, &s_bitmap, 0x7f, 0x7f, 0x7f, 0xff);
        if (s32Ret != TS_SUCCESS) {
            __ERR("SAMPLE_COMM_REGION_Alloc_Paint_Bitmap failed %d!\n", s32Ret);
            return TS_FAILURE;
        }

        for (i = 0; i < 8; ++i) {
            stChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = i * (FF_REGION_W + FF_OSD_LINEWIDTH);

            s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegion);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_SetBitMap(g_ffHdl, &s_bitmap);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttr);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            g_hdlArray[ts32HdlArrayIdx++] = g_ffHdl;
            g_ffHdl++;
        }
    }

    if (type == 1) {
        for (i = 0; i < 8; ++i) {
            stChnAttrCover.unChnAttr.stCoverChn.stRect.s32X = i * (FF_REGION_W + FF_OSD_LINEWIDTH);
            stChnAttrCover.unChnAttr.stCoverChn.u32Color = (TS_U32)0xff << ((i % 4) * 8);
            printf("color argb 0x%x", stChnAttrCover.unChnAttr.stCoverChn.u32Color);

            s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegionCover);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttrCover);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            g_hdlArray[ts32HdlArrayIdx++] = g_ffHdl;
            g_ffHdl++;
        }
    }

    if (type == 2) {
        for (i = 0; i < 8; ++i) {
            stChnAttrMosaic.unChnAttr.stMosaicChn.stRect.s32X = i * (FF_REGION_W + FF_OSD_LINEWIDTH);
            stChnAttrMosaic.unChnAttr.stMosaicChn.enBlkSize = i;
            if (stChnAttrMosaic.unChnAttr.stMosaicChn.enBlkSize >= MOSAIC_BLK_SIZE_BUTT) {
                stChnAttrMosaic.unChnAttr.stMosaicChn.enBlkSize = MOSAIC_BLK_SIZE_64;
            }

            s32Ret = TS_MPI_RGN_Create(g_ffHdl, &stRegionMosaic);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            s32Ret = TS_MPI_RGN_AttachToChn(g_ffHdl, pstMppChn, &stChnAttrMosaic);
            if (s32Ret != TS_SUCCESS) {
                __ERR("TS_MPI_RGN_Create failed %d!\n", s32Ret);
                return TS_FAILURE;
            }

            g_hdlArray[ts32HdlArrayIdx++] = g_ffHdl;
            g_ffHdl++;
        }
    }

    for (i = 0; i < OSD_THREAD_PARAM_NUM; ++i) {
        LOG_D("hdlArray:\n");
        LOG_D("%d \n", g_hdlArray[i]);
        LOG_D("\n");
    }

    return TS_SUCCESS;
}

TS_S32 SAMPLE_COMM_REGION_Stop_Rgn(MPP_CHN_S *pstMppChn)
{
    TS_S32 s32Ret;
    TS_S32 i;
    RGN_HANDLE tmpHdl;

    for (i = 0; i < OSD_THREAD_PARAM_NUM; ++i) {
        tmpHdl = g_hdlArray[i];
        if (tmpHdl == -1) {
            continue;
        }

        s32Ret = TS_MPI_RGN_DetachFromChn(tmpHdl, pstMppChn);
        if (TS_SUCCESS != s32Ret) {
            __ERR("APP_REGION_DetachFromChn failed! Handle:%d\n", tmpHdl);
            return TS_FAILURE;
        }
        s32Ret = TS_MPI_RGN_Destroy(tmpHdl);
        if (TS_SUCCESS != s32Ret) {
            __ERR("TS_MPI_RGN_Destroy failed! Handle:%d\n", tmpHdl);
            return TS_FAILURE;
        }

        g_hdlArray[i] = -1;
    }

    //TODO
    //	free(s_bitmap.pData);
    //	s_bitmap.pData = NULL;

    return TS_SUCCESS;
}
#endif