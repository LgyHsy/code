#include "anj_mw_media_common.h"

#define TS_COMM_OSD_MAX_HANDLE (512)

static RGN_CANVAS_INFO_S g_stCanvasInfo;

TS_S32 TS_Common_OsdCanvasUpdate(RGN_HANDLE hHandle)
{
    if (hHandle >= (RGN_HANDLE)TS_COMM_OSD_MAX_HANDLE)
    {
        __ERR("OSD handle error, hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }

    return TS_MPI_RGN_UpdateCanvas(hHandle);
}

TS_S32 TS_Common_OsdCanvasGet(RGN_HANDLE hHandle, RGN_CANVAS_INFO_S **ppstRgnCanvasInfo)
{
    TS_S32 s32Ret = TS_FAILURE;

    if (hHandle >= (RGN_HANDLE)TS_COMM_OSD_MAX_HANDLE)
    {
        __ERR("OSD handle error, hHandle=%u\n", (unsigned int)hHandle);
        return TS_FAILURE;
    }
    if (ppstRgnCanvasInfo == NULL)
    {
        __ERR("ppstRgnCanvasInfo is null\n");
        return TS_FAILURE;
    }

    if (*ppstRgnCanvasInfo == NULL)
    {
        s32Ret = TS_MPI_RGN_GetCanvasInfo(hHandle, &g_stCanvasInfo);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_RGN_GetCanvasInfo failed: 0x%x, h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
            return s32Ret;
        }
        *ppstRgnCanvasInfo = &g_stCanvasInfo;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_OsdAttrGet(RGN_HANDLE hHandle, RGN_ATTR_S *pstRgnAttr)
{
    if (pstRgnAttr == NULL)
    {
        __ERR("pstRgnAttr is null\n");
        return TS_FAILURE;
    }

    return TS_MPI_RGN_GetAttr(hHandle, pstRgnAttr);
}

TS_S32 TS_Common_OsdDisplayAttrGet(RGN_HANDLE hHandle, MPP_CHN_S *pstChnPort, RGN_CHN_ATTR_S *pstChnPortAttr)
{
    if (pstChnPort == NULL || pstChnPortAttr == NULL)
    {
        __ERR("input pointer is null\n");
        return TS_FAILURE;
    }

    return TS_MPI_RGN_GetDisplayAttr(hHandle, pstChnPort, pstChnPortAttr);
}

TS_VOID TS_Common_OsdSetBitmap(RGN_HANDLE hHandle, PIXEL_FORMAT_E enPix, TS_U32 u32W, TS_U32 u32H, MPP_CHN_S *pstRgnChnPort)
{
    BITMAP_S stBitmap = {0};
    stBitmap.enPixelFormat = enPix;
    stBitmap.u32Width = u32W;
    stBitmap.u32Height = u32H;

    size_t alloc_len = 0;
    if (enPix == PIXEL_FORMAT_ARGB_2BIT || enPix == PIXEL_FORMAT_ARGB_2BPP)
    {
        alloc_len = (size_t)u32W * (size_t)u32H / 4; // 2bit 每像素 2bit
    }
    else if (enPix == PIXEL_FORMAT_ARGB_4BIT)
    {
        alloc_len = (size_t)u32W * (size_t)u32H / 2; // 4bit 每像素 4bit
    }
    else if (enPix == PIXEL_FORMAT_ARGB_1555)
    {
        alloc_len = (size_t)u32W * (size_t)u32H * 2; // 1555 每像素 16bit
    }
    else if (enPix == PIXEL_FORMAT_ARGB_4444)
    {
        alloc_len = (size_t)u32W * (size_t)u32H * 2; // 4444 每像素 16bit
    }

    if (alloc_len > 0)
    {
        TS_U8 *pInit = (TS_U8 *)anj_mw_malloc(alloc_len);
        if (pInit)
        {
            memset(pInit, 0, alloc_len);
            stBitmap.pData = pInit;
            TS_MPI_RGN_SetBitMap(hHandle, &stBitmap);
            anj_mw_free(pInit);
        }
    }

    // 对齐 OSD_Timestamp：ARGB_2BIT 需要给 VENC 配 ROI，才能在编码流里稳定可见
    if (enPix == PIXEL_FORMAT_ARGB_2BIT && pstRgnChnPort->s32ChnId == 0)
    {
        VENC_ROI_ATTR_S stRoiAttr = {0};
        TS_MPI_VENC_GetRoiAttr((VENC_CHN)pstRgnChnPort->s32DevId, 0, &stRoiAttr);
        stRoiAttr.bAbsQp = TS_FALSE;
        stRoiAttr.bEnable = 1;
        stRoiAttr.s32Qp = 37;
        stRoiAttr.u32Index = 0;
        stRoiAttr.stRect.u32Height = ANJ_ALIGN_UP(u32H, 16);
        stRoiAttr.stRect.u32Width = ANJ_ALIGN_UP(u32W, 16);
        TS_MPI_VENC_SetRoiAttr((VENC_CHN)pstRgnChnPort->s32DevId, &stRoiAttr);
    }
}

TS_S32 TS_Common_OsdCreate(RGN_HANDLE hHandle, RGN_ATTR_S *pstRgnAttr, MPP_CHN_S *pstRgnChnPort,
                           RGN_CHN_ATTR_S *pstRgnChnPortParam)
{
    TS_S32 s32Ret = TS_FAILURE;

    if (pstRgnAttr == NULL || pstRgnChnPort == NULL || pstRgnChnPortParam == NULL)
    {
        __ERR("input pointer is null\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_RGN_Create(hHandle, pstRgnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_RGN_Create failed: 0x%x, h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return s32Ret;
    }

    s32Ret = TS_MPI_RGN_AttachToChn(hHandle, pstRgnChnPort, pstRgnChnPortParam);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_RGN_AttachToChn failed: 0x%x, h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        (void)TS_MPI_RGN_Destroy(hHandle);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_OsdDestory(RGN_HANDLE hHandle, MPP_CHN_S *pstRgnChnPort)
{
    TS_S32 s32Ret = TS_SUCCESS;

    if (pstRgnChnPort == NULL)
    {
        __ERR("pstRgnChnPort is null\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_RGN_DetachFromChn(hHandle, pstRgnChnPort);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_RGN_DetachFromChn failed: 0x%x, h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return s32Ret;
    }

    s32Ret = TS_MPI_RGN_Destroy(hHandle);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_RGN_Destroy failed: 0x%x, h=%u\n", (unsigned int)s32Ret, (unsigned int)hHandle);
        return s32Ret;
    }

    return TS_SUCCESS;
}

TS_S32 TS_Common_OsdRgnInit(int pixel_fmt)
{
    return TS_SUCCESS;
}

TS_S32 TS_Common_OsdRgnUnInit(void)
{
    return TS_SUCCESS;
}
