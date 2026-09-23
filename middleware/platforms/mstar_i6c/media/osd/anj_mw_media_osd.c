#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"

#include "anj_mw_media_common.h"
#include "anj_mw_media_osd.h"

#define ANJ_MW_OSD_DRAW_RGN_BASE (MAX_VIDEO_NUM * OSD_TYPE_MAX + MAX_RGN_MASK_AREA * MAX_VIDEO_NUM)
#define ANJ_MW_OSD_LINE_RGN_HANDLE(stream) (ANJ_MW_OSD_DRAW_RGN_BASE + (stream))
#define ANJ_MW_OSD_RECT_RGN_HANDLE(stream) (ANJ_MW_OSD_DRAW_RGN_BASE + MAX_VIDEO_NUM + (stream))

static pthread_mutex_t s_stMutexOsdUptState = PTHREAD_MUTEX_INITIALIZER;

void DrawPoint(void *pBaseAddr, MI_U32 u32Stride, MI_U32 u32Height, MI_U32 u32Width, DrawPoint_t stPt, DrawRgnColor_t stColor)
{
    if (pBaseAddr == NULL)
        return;

    /* basic bounds check to avoid out-of-range writes */
    if (stPt.u16X < 0 || stPt.u16Y < 0)
        return;
    if ((MI_U32)stPt.u16X > u32Width || (MI_U32)stPt.u16Y > u32Height)
        return;

    switch (stColor.ePixelFmt)
    {
    case E_MI_RGN_PIXEL_FORMAT_I2:
    {
        /* I2格式：4像素/字节 (2bpp) */
        if (u32Stride == 0)
            return;
        MI_U32 stride_bytes = u32Stride;
        MI_U32 byte_pos = (MI_U32)stPt.u16Y * stride_bytes + (stPt.u16X / 4);

        /* ensure byte_pos is within buffer (stride * height) */
        if (byte_pos >= stride_bytes * u32Height)
            return;

        MI_U8 *pCur = (MI_U8 *)pBaseAddr + byte_pos;
        MI_U8 shift = (stPt.u16X % 4) * 2;
        *pCur = (*pCur & ~(0x03 << shift)) | ((stColor.u32Color & 0x03) << shift);
    }
    break;
    case E_MI_RGN_PIXEL_FORMAT_I4:
    {
        /* I4格式：2像素/字节 (4bpp) */
        MI_U32 stride_bytes = u32Stride;
        MI_U32 byte_pos = stPt.u16Y * stride_bytes + (stPt.u16X / 2);

        MI_U8 *pCur = (MI_U8 *)pBaseAddr + byte_pos;
        if (stPt.u16X % 2)
            *pCur = (*pCur & 0x0F) | ((stColor.u32Color & 0x0F) << 4);
        else
            *pCur = (*pCur & 0xF0) | (stColor.u32Color & 0x0F);
    }
    break;
    case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
    {
        /* ARGB1555格式：2字节/像素 */
        MI_U32 stride_bytes = u32Stride;
        MI_U32 byte_pos = stPt.u16Y * stride_bytes + stPt.u16X * 2;

        MI_U8 *pCur = (MI_U8 *)pBaseAddr + byte_pos;
        *pCur = (stColor.u32Color >> 8) & 0xFF;
        *(pCur + 1) = stColor.u32Color & 0xFF;
    }
    break;

    default:
        __INFO("format not support\n");
    }
}

void DrawLine(void *pBaseAddr, MI_U32 u32Stride, MI_U32 u32Height, MI_U32 u32Width, DrawPoint_t stStartPt, DrawPoint_t stEndPt, MI_U8 u8BorderWidth, DrawRgnColor_t stColor)
{
    if (pBaseAddr == NULL)
    {
        return;
    }
    MI_S16 x0 = stStartPt.u16X;
    MI_S16 y0 = stStartPt.u16Y;
    MI_S16 x1 = stEndPt.u16X;
    MI_S16 y1 = stEndPt.u16Y;

    // 计算线宽分配（左右/上下偏移量）
    MI_S16 width_left = 0;
    MI_S16 width_right = 0;
    if ((u8BorderWidth % 2) != 0)
    {
        width_left = (u8BorderWidth >> 1);
        width_right = (u8BorderWidth >> 1) + 1;
    }
    else
    {
        width_left = (u8BorderWidth >> 1);
        width_right = width_left;
    }

    // 处理水平线边界
    if (y0 == y1)
    {
        y0 = CLAMP(y0, width_left, (MI_S16)(u32Height - width_right));
        y1 = y0;
    }
    // 处理垂直线边界
    if (x0 == x1)
    {
        x0 = CLAMP(x0, width_left, (MI_S16)(u32Width - width_right));
        x1 = x0;
    }

    // 计算斜率和方向
    MI_S16 dx = abs(x1 - x0);
    MI_S16 dy = abs(y1 - y0);
    MI_BOOL bSteep = (dy > dx) ? 1 : 0; // 是否为陡峭斜率

    if (x0 > x1)
    {
        SWAP(x0, x1);
        SWAP(y0, y1);
    }

    int deltax = x1 - x0;
    int deltay = y1 - y0;
    if (0 == bSteep)
    { // 平缓斜率（|dx| >= |dy|）
        float y = y0;
        float ystep = (float)deltay / (float)deltax;

        for (MI_S16 x = x0; x <= x1; x++)
        {
            int pointY = (int)y;

            for (MI_S16 jIndex = 0 - width_left; jIndex < width_right; jIndex++)
            {
                pointY = (int)y + jIndex;
                if (pointY < 0)
                {
                    continue;
                }

                if (pointY >= u32Height)
                    pointY = u32Height - 1;

                // draw_pixel(stColor.u32Color, x, pointY, u32Stride, u32Height, pBaseAddr);
                DrawPoint_t stPt = {0};
                stPt.u16X = x;
                stPt.u16Y = pointY;
                DrawPoint(pBaseAddr, u32Stride, u32Height, u32Width, stPt, stColor);
            }
            y += ystep;
        }
    }
    else
    { // 陡峭斜率（|dy| > |dx|）

        if (y0 > y1)
        {
            SWAP(y0, y1);
            SWAP(x0, x1);
        }
        float x = x0;
        float xstep = (float)deltax / (float)deltay;

        for (int y = y0; y < y1; y++)
        {
            int pointX = (int)x;
            for (int jIndex = 0 - width_left; jIndex < width_right; jIndex++)
            {
                pointX = (int)x + jIndex;

                if (pointX < 0)
                {
                    continue;
                }

                if (pointX >= u32Width)
                    pointX = u32Width - 1;
                // draw_pixel(stColor.u32Color, pointX, y, u32Stride, u32Height, pBaseAddr);

                DrawPoint_t stPt = {0};
                stPt.u16X = pointX;
                stPt.u16Y = y;
                DrawPoint(pBaseAddr, u32Stride, u32Height, u32Width, stPt, stColor);
            }

            x += xstep;
        }
    }
}

void DrawRect(void *pBaseAddr, MI_U32 u32Stride, DrawPoint_t stLeftTopPt, DrawPoint_t stRightBottomPt, MI_U8 u8BorderWidth, DrawRgnColor_t stColor)
{
    MI_U32 i = 0;
    MI_U32 j = 0;
    MI_U32 u32Width = stRightBottomPt.u16X - stLeftTopPt.u16X + 1;
    MI_U32 u32Height = stRightBottomPt.u16Y - stLeftTopPt.u16Y + 1;

    if ((u8BorderWidth > u32Width / 2) || (u8BorderWidth > u32Height / 2))
    {
        printf("invalid border width\n");
        return;
    }

    switch (stColor.ePixelFmt)
    {
    case E_MI_RGN_PIXEL_FORMAT_I2:
    {
        MI_U8 *pDrawBase = (MI_U8 *)pBaseAddr;
        if (stLeftTopPt.u16X % 4 || stRightBottomPt.u16X % 4 || (u8BorderWidth > u32Width / 4) || (u8BorderWidth > u32Height / 4))
        {
            printf("invalid rect position\n");
            return;
        }
        for (i = 0; i < u32Width / 4; i++)
        {
            for (j = 0; j < u8BorderWidth && ((stLeftTopPt.u16X / 4 + i) < u32Stride); j++)
            {
                *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + j) + stLeftTopPt.u16X / 4 + i) = (stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2) | ((stColor.u32Color & 0x03) << 4) | ((stColor.u32Color & 0x03) << 6);     // copy 1 byte
                *(pDrawBase + u32Stride * (stRightBottomPt.u16Y - j) + stLeftTopPt.u16X / 4 + i) = (stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2) | ((stColor.u32Color & 0x03) << 4) | ((stColor.u32Color & 0x03) << 6); // copy 1 byte
            }
        }
        for (i = 0; i < u32Height; i++)
        {
            for (j = 0; j < u8BorderWidth / 4; j++)
            {
                if ((stLeftTopPt.u16X / 4 + j) < u32Stride)
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 4 + j) = (stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2) | ((stColor.u32Color & 0x03) << 4) | ((stColor.u32Color & 0x03) << 6); // copy 1 byte
                }
                if ((stRightBottomPt.u16X / 4 - j) < u32Stride)
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 4 - j) = (stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2) | ((stColor.u32Color & 0x03) << 4) | ((stColor.u32Color & 0x03) << 6); // copy 1 byte
                }
            }
            if (u8BorderWidth % 4)
            {
                if (((stLeftTopPt.u16X / 4 + u8BorderWidth / 4) < u32Stride))
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 4 + u8BorderWidth / 4) &= 0xf0;
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 4 + u8BorderWidth / 4) |= (stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2);
                }
                if (((stRightBottomPt.u16X / 4 - u8BorderWidth / 4) < u32Stride))
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 4 - u8BorderWidth / 4) &= 0x0f;
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 4 - u8BorderWidth / 4) |= ((stColor.u32Color & 0x03) | ((stColor.u32Color & 0x03) << 2)) << 4;
                }
            }
        }
    }
    break;
    case E_MI_RGN_PIXEL_FORMAT_I4:
    {
        MI_U8 *pDrawBase = (MI_U8 *)pBaseAddr;

        if (stLeftTopPt.u16X % 2 || stRightBottomPt.u16X % 2)
        {
            printf("invalid rect position\n");
            return;
        }

        for (i = 0; i < u32Width / 2; i++)
        {
            for (j = 0; j < u8BorderWidth && ((stLeftTopPt.u16X / 2 + i) < u32Stride); j++)
            {

                *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + j) + stLeftTopPt.u16X / 2 + i) = (stColor.u32Color & 0x0f) | ((stColor.u32Color & 0x0f) << 4);     // copy 1 byte
                *(pDrawBase + u32Stride * (stRightBottomPt.u16Y - j) + stLeftTopPt.u16X / 2 + i) = (stColor.u32Color & 0x0f) | ((stColor.u32Color & 0x0f) << 4); // copy 1 byte
            }
        }

        for (i = 0; i < u32Height; i++)
        {
            for (j = 0; j < u8BorderWidth / 2; j++)
            {
                if ((stLeftTopPt.u16X / 2 + j) < u32Stride)
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 2 + j) = (stColor.u32Color & 0x0f) | ((stColor.u32Color & 0x0f) << 4); // copy 1 byte
                }

                if ((stRightBottomPt.u16X / 2 - j) < u32Stride)
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 2 - j) = (stColor.u32Color & 0x0f) | ((stColor.u32Color & 0x0f) << 4); // copy 1 byte
                }
            }

            if (u8BorderWidth % 2)
            {
                if (((stLeftTopPt.u16X / 2 + u8BorderWidth / 2) < u32Stride))
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 2 + u8BorderWidth / 2) &= 0xf0;
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X / 2 + u8BorderWidth / 2) |= stColor.u32Color & 0x0f;
                }

                if (((stRightBottomPt.u16X / 2 - u8BorderWidth / 2) < u32Stride))
                {
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 2 - u8BorderWidth / 2) &= 0x0f;
                    *(pDrawBase + u32Stride * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X / 2 - u8BorderWidth / 2) |= (stColor.u32Color & 0x0f) << 4;
                }
            }
        }
    }
    break;
    case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
    {
        MI_U16 *pDrawBase = (MI_U16 *)pBaseAddr;

        for (i = 0; i < u32Width; i++)
        {
            for (j = 0; j < u8BorderWidth && (stLeftTopPt.u16X + i) < u32Stride / 2; j++)
            {
                *(pDrawBase + u32Stride / 2 * (stLeftTopPt.u16Y + j) + stLeftTopPt.u16X + i) = stColor.u32Color & 0xffff;     // copy 2 byte, app check alignment
                *(pDrawBase + u32Stride / 2 * (stRightBottomPt.u16Y - j) + stLeftTopPt.u16X + i) = stColor.u32Color & 0xffff; // copy 2 byte, app check alignment
            }
        }

        for (i = 0; i < u32Height; i++)
        {
            for (j = 0; j < u8BorderWidth; j++)
            {
                if ((stLeftTopPt.u16X + j) < u32Stride / 2)
                {
                    *(pDrawBase + u32Stride / 2 * (stLeftTopPt.u16Y + i) + stLeftTopPt.u16X + j) = stColor.u32Color & 0xffff; // copy 2 byte, app check alignment
                }

                if ((stRightBottomPt.u16X - j) < u32Stride / 2)
                {
                    *(pDrawBase + u32Stride / 2 * (stLeftTopPt.u16Y + i) + stRightBottomPt.u16X - j) = stColor.u32Color & 0xffff; // copy 2 byte, app check alignment
                }
            }
        }
    }
    break;
    default:
        __ERR("format not support\n");
    }
}

static int anj_mw_osd_draw_canvas(MI_RGN_CanvasInfo_t *pstRgnCanvasInfo, overlay_param_s *overlay_param)
{
    if (overlay_param == NULL || overlay_param->bitmap_data == NULL ||
        pstRgnCanvasInfo == NULL || pstRgnCanvasInfo->virtAddr == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    int tmp_x = overlay_param->pos_x;
    int tmp_y = overlay_param->pos_y;
    int tmp_w = overlay_param->width;
    int tmp_h = overlay_param->height;
    MI_U32 u32Stride = pstRgnCanvasInfo->u32Stride;

    int alignment = 1;
    switch (pstRgnCanvasInfo->ePixelFmt)
    {
    case E_MI_RGN_PIXEL_FORMAT_I2:
        alignment = 4;
        break;
    case E_MI_RGN_PIXEL_FORMAT_I4:
        alignment = 2;
        break;
    default:
        alignment = 1;
        break;
    }

    int src_width = overlay_param->width;
    int src_aligned_width = ANJ_ALIGN_UP(src_width, alignment);
    int dst_aligned_width = ANJ_ALIGN_UP(pstRgnCanvasInfo->stSize.u32Width, alignment);

    if ((int)pstRgnCanvasInfo->stSize.u32Width == overlay_param->width &&
        (int)pstRgnCanvasInfo->stSize.u32Height == overlay_param->height)
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
    switch (pstRgnCanvasInfo->ePixelFmt)
    {
    case E_MI_RGN_PIXEL_FORMAT_I2:
        tmp_x = tmp_x >> 2;
        tmp_w = src_aligned_width >> 2;
        dst_bytes_per_pixel = 0;
        break;
    case E_MI_RGN_PIXEL_FORMAT_I4:
        tmp_x = tmp_x >> 1;
        tmp_w = src_aligned_width >> 1;
        dst_bytes_per_pixel = 0;
        break;
    case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
        tmp_x *= 2;
        tmp_w *= 2;
        dst_bytes_per_pixel = 2;
        break;
    default:
        __ERR("Unsupported pixel format: %d\n", pstRgnCanvasInfo->ePixelFmt);
        return E_MI_ERR_FAILED;
    }

    if (tmp_x < 0 || tmp_y < 0 ||
        tmp_x + tmp_w > dst_aligned_width ||
        tmp_y + tmp_h > pstRgnCanvasInfo->stSize.u32Height)
    {
        __ERR("Canvas area out of bound: (%d,%d)-(%d,%d) vs (%d,%d)\n",
              tmp_x, tmp_y, tmp_w, tmp_h,
              dst_aligned_width, pstRgnCanvasInfo->stSize.u32Height);
        return E_MI_ERR_FAILED;
    }

    MI_U32 byte_pos = tmp_y * u32Stride + tmp_x;
    MI_U8 *pCur = (MI_U8 *)pstRgnCanvasInfo->virtAddr + byte_pos;
    MI_U8 *u8SrcBuf = (MI_U8 *)overlay_param->bitmap_data;

    for (int h = 0; h < tmp_h; h++)
    {
        if ((pCur + tmp_w > (MI_U8 *)pstRgnCanvasInfo->virtAddr + u32Stride * pstRgnCanvasInfo->stSize.u32Height) ||
            (u8SrcBuf + tmp_w > (MI_U8 *)overlay_param->bitmap_data + src_aligned_width * overlay_param->height * (dst_bytes_per_pixel ? 2 : 1)))
        {
            __WARN("Memory copy out of bound at line %d\n", h);
            break;
        }

        memcpy(pCur, u8SrcBuf, tmp_w);
        pCur += u32Stride;
        u8SrcBuf += tmp_w;
    }

    return MI_SUCCESS;
}

int anj_mw_osd_init(int pixel_fmt)
{
    return ST_Common_OsdRgnInit(pixel_fmt);
}

int anj_mw_osd_uninit()
{
    return ST_Common_OsdRgnUnInit();
}

int anj_mw_osd_create(overlay_param_s *overlay_param, ANJ_SIZE_S *resolution, int rgn_pixel_format)
{
    MI_S32 s32Ret = MI_SUCCESS;
    MI_RGN_ChnPort_t m_stRgnChnPort;
    // MI_SCL_OutPortParam_t m_stSclOutputPortParam;
    MI_RGN_Size_t m_stRgnSize;
    MI_RGN_Point_t m_stRgnPoint;

    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    MI_S32 u32OsdHandle = overlay_param->rgn_handle;
    if (((MI_S32)u32OsdHandle <= MI_RGN_HANDLE_NULL) || (u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", u32OsdHandle);
        return MI_SUCCESS;
    }

    memset(&m_stRgnChnPort, 0, sizeof(m_stRgnChnPort));
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_VENC;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = overlay_param->stream_type;
    m_stRgnChnPort.s32PortId = 0;

    if (OSD_FULL_IMAGE)
    {
        if (u32OsdHandle != (overlay_param->stream_type * OSD_TYPE_MAX))
        {
            __WARN("u32OsdHandle:%d %d\n", u32OsdHandle, (u32OsdHandle % OSD_TYPE_MAX));
            return MI_SUCCESS;
        }
        m_stRgnSize.u32Width = resolution->u32Width;
        m_stRgnSize.u32Height = resolution->u32Height;
        m_stRgnPoint.u32X = 0;
        m_stRgnPoint.u32Y = 0;
    }
    else
    {
        m_stRgnSize.u32Width = overlay_param->width;
        m_stRgnSize.u32Height = overlay_param->height;
        m_stRgnPoint.u32X = overlay_param->pos_x;
        m_stRgnPoint.u32Y = overlay_param->pos_y;
    }

    MI_RGN_Attr_t stRgnAttr;
    memset(&stRgnAttr, 0x00, sizeof(MI_RGN_Attr_t));
    stRgnAttr.eType = E_MI_RGN_TYPE_OSD;
    stRgnAttr.stOsdInitParam.ePixelFmt = E_MI_RGN_PIXEL_FORMAT_I4;
    if (rgn_pixel_format == PIXEL_FORMAT_I2)
    {
        stRgnAttr.stOsdInitParam.ePixelFmt = E_MI_RGN_PIXEL_FORMAT_I2;
    }
    stRgnAttr.stOsdInitParam.stSize.u32Width = m_stRgnSize.u32Width;
    stRgnAttr.stOsdInitParam.stSize.u32Height = m_stRgnSize.u32Height;
    // stRgnAttr.stOsdInitParam.u16MaxCanvasNum = MAX_OSD_CANVAS_NUM;

    MI_RGN_ChnPortParam_t stRgnChnPortParam;
    memset(&stRgnChnPortParam, 0x00, sizeof(MI_RGN_ChnPortParam_t));
    stRgnChnPortParam.bShow = TRUE;
    stRgnChnPortParam.stPoint.u32X = m_stRgnPoint.u32X;
    stRgnChnPortParam.stPoint.u32Y = m_stRgnPoint.u32Y;
    stRgnChnPortParam.unPara.stOsdChnPort.u32Layer =
        OSD_FULL_IMAGE ? (overlay_param->stream_type * OSD_TYPE_MAX) : overlay_param->rgn_handle;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.eAlphaMode = E_MI_RGN_PIXEL_ALPHA;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8BgAlpha = 0x0;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8FgAlpha = 0xAA;

    /* 与 canvas Get/Draw/Update 互斥，避免 rgn 建立/销毁与 MI_RGN_UpdateCanvas
     * 的内核 cache flush 并发（会触发 sgs_mi 内核 Oops）。*/
    anj_mutex_lock(&s_stMutexOsdUptState);
    s32Ret = ST_Common_OsdCreate(u32OsdHandle, &stRgnAttr, &m_stRgnChnPort, &stRgnChnPortParam);
    anj_mutex_unlock(&s_stMutexOsdUptState);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("ST_Common_OsdCreate() error(0x%X)\n", s32Ret);
    }

    char cmd[128] = {0};
    snprintf(cmd, sizeof(cmd), "echo setMaxCanvasForHandle %d %d >  /proc/mi_modules/mi_rgn/mi_rgn0", u32OsdHandle, MAX_OSD_HANDLE_CANVAS);
    anj_mw_system(cmd);

    s32Ret = anj_mw_osd_canvas_info_clear(overlay_param);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("anj_mw_osd_canvas_info_clear() error(0x%X)\n", s32Ret);
    }

    __INFO("rgn %d create sucess!\n", u32OsdHandle);

    return s32Ret;
}

int anj_mw_osd_destroy(overlay_param_s *overlay_param)
{
    MI_S32 s32Ret = MI_SUCCESS;

    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    MI_S32 u32OsdHandle = overlay_param->rgn_handle;
    if ((u32OsdHandle <= MI_RGN_HANDLE_NULL || u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    if (OSD_FULL_IMAGE && u32OsdHandle != (overlay_param->stream_type * OSD_TYPE_MAX))
    {
        return s32Ret;
    }

    MI_RGN_ChnPort_t m_stRgnChnPort = {0};
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_VENC;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = overlay_param->stream_type;
    m_stRgnChnPort.s32PortId = 0;

    s32Ret = anj_mw_osd_canvas_info_clear(overlay_param);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("anj_mw_osd_clean_data() error(0x%X)\n", s32Ret);
    }

    /* MI_RGN_Destroy 会 munmap canvas；必须与持锁进行中的
     * GetCanvasInfo/DrawRect/UpdateCanvas（如 rm_vyuv 线程画检测框）互斥，
     * 否则内核对 canvas 做 cache flush 时映射被解除，触发 kernel Oops。*/
    anj_mutex_lock(&s_stMutexOsdUptState);
    s32Ret = ST_Common_OsdDestory(u32OsdHandle, &m_stRgnChnPort);
    anj_mutex_unlock(&s_stMutexOsdUptState);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("ST_Common_OsdDestory error(0x%X), hdl=%d\n", s32Ret, u32OsdHandle);
        return s32Ret;
    }

    return s32Ret;
}

int anj_mw_osd_bitmap_data_free(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    if (overlay_param->bitmap_data)
    {
        anj_mw_free(overlay_param->bitmap_data);
        overlay_param->bitmap_data = NULL;
        overlay_param->bitmap_len = 0;
    }

    return MI_SUCCESS;
}

int anj_mw_osd_bitmap_data_malloc(rgn_pixel_format_e ePixelFmt, overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    int len = 0;
    switch (ePixelFmt)
    {
    case PIXEL_FORMAT_I2:
    {
        len = overlay_param->height * overlay_param->width / 4;
        break;
    }
    case PIXEL_FORMAT_I4:
    {
        len = overlay_param->height * overlay_param->width / 2;
        break;
    }
    case PIXEL_FORMAT_ARGB1555:
    {
        len = overlay_param->height * overlay_param->width * 2;
        break;
    }

    default:
        break;
    }
    if (overlay_param->bitmap_data)
    {
        if (len > overlay_param->bitmap_len)
        {
            overlay_param->bitmap_data = anj_mw_realloc(overlay_param->bitmap_data, len);
            overlay_param->bitmap_len = len;
        }
    }
    else if (len > 0)
    {
        overlay_param->bitmap_data = anj_mw_malloc(len);
        overlay_param->bitmap_len = len;
    }

    if (overlay_param->bitmap_data == NULL || overlay_param->bitmap_len <= 0)
    {
        __ERR("bitmap_data[%d] is null\n", overlay_param->rgn_handle);
        return E_MI_ERR_FAILED;
    }

    return MI_SUCCESS;
}

int anj_mw_osd_draw_pixel(rgn_pixel_format_e ePixelFmt, unsigned short value,
                          int offset_x, int offset_y, overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    if (overlay_param->bitmap_data == NULL)
    {
        return MI_SUCCESS;
    }
    int canvas_w = overlay_param->width;
    int canvas_h = overlay_param->height;
    if (offset_x < 0 || offset_y < 0 ||
        offset_x >= canvas_w || offset_y >= canvas_h)
    {
        __ERR("Coordinate out of range: (%d, %d) vs (%d x %d)\n",
              offset_x, offset_y, canvas_w, canvas_h);
        return E_MI_ERR_FAILED;
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
        return E_MI_ERR_FAILED;
    }

    int aligned_width = ANJ_ALIGN_UP(canvas_w, alignment);

    MI_U8 *pCur = (MI_U8 *)overlay_param->bitmap_data;

    switch (ePixelFmt)
    {
    case PIXEL_FORMAT_I2:
    {
        /* I2格式：4像素/字节 (2bpp) */
        MI_U32 stride_bytes = aligned_width / 4;
        MI_U32 byte_pos = offset_y * stride_bytes + (offset_x / 4);
        if (byte_pos >= (aligned_width * canvas_h))
        {
            break;
        }
        pCur = (MI_U8 *)overlay_param->bitmap_data + byte_pos;
        MI_U8 shift = (offset_x % 4) * 2;
        *pCur = (*pCur & ~(0x03 << shift)) | ((value & 0x03) << shift);
        break;
    }
    case PIXEL_FORMAT_I4:
    {
        /* I4格式：2像素/字节 (4bpp) */
        MI_U32 stride_bytes = aligned_width / 2;
        MI_U32 byte_pos = offset_y * stride_bytes + (offset_x / 2);
        if (byte_pos >= (aligned_width * canvas_h / 2))
        {
            break;
        }

        pCur = (MI_U8 *)overlay_param->bitmap_data + byte_pos;
        if (offset_x % 2)
            *pCur = (*pCur & 0x0F) | ((value & 0x0F) << 4);
        else
            *pCur = (*pCur & 0xF0) | (value & 0x0F);
        break;
    }
    case PIXEL_FORMAT_ARGB1555:
    {
        /* ARGB1555格式：2字节/像素 */
        MI_U32 stride_bytes = aligned_width * 2;
        MI_U32 byte_pos = offset_y * stride_bytes + offset_x * 2;
        if (byte_pos + 1 >= (aligned_width * canvas_h * 2))
        {
            break;
        }

        pCur = (MI_U8 *)overlay_param->bitmap_data + byte_pos;
        *pCur = (value >> 8) & 0xFF;
        *(pCur + 1) = value & 0xFF;
        break;
    }
    default:
        break;
    }

    return MI_SUCCESS;
}

int anj_mw_osd_canvas_update(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    if (overlay_param->bitmap_data == NULL)
    {
        return MI_SUCCESS;
    }

    MI_S32 u32OsdHandle = overlay_param->rgn_handle;
    if (((MI_S32)u32OsdHandle <= MI_RGN_HANDLE_NULL || u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,u32OsdHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    int iRgnCanvasIndex;
    if (OSD_FULL_IMAGE)
    {
        iRgnCanvasIndex = overlay_param->stream_type * OSD_TYPE_MAX;
    }
    else
    {
        iRgnCanvasIndex = overlay_param->rgn_handle;
        MI_RGN_Attr_t stRgnAttr = {0};
        if (0 != ST_Common_OsdAttrGet(iRgnCanvasIndex, &stRgnAttr))
        {
            __ERR("OSD handle error,u32OsdHandle=%d\n", iRgnCanvasIndex);
            return E_MI_ERR_FAILED;
        }
        MI_RGN_ChnPort_t stChnPort = {0};
        stChnPort.eModId = E_MI_MODULE_ID_VENC;
        stChnPort.s32DevId = 0;
        stChnPort.s32ChnId = overlay_param->stream_type;
        stChnPort.s32PortId = 0;
        MI_RGN_ChnPortParam_t stChnPortAttr = {0};
        if (0 != ST_Common_OsdDisplayAttrGet(iRgnCanvasIndex, &stChnPort, &stChnPortAttr))
        {
            __ERR("OSD handle error,u32OsdHandle=%d\n", iRgnCanvasIndex);
            return E_MI_ERR_FAILED;
        }
        if (stRgnAttr.stOsdInitParam.stSize.u32Width != overlay_param->width ||
            stRgnAttr.stOsdInitParam.stSize.u32Height != overlay_param->height ||
            stChnPortAttr.stPoint.u32X != overlay_param->pos_x ||
            stChnPortAttr.stPoint.u32Y != overlay_param->pos_y)
        {
            // __INFO("Recreate RGN handle %d for size change %dx%d %dx%d -> %dx%d %dx%d\n",
            //        iRgnCanvasIndex,
            //        stChnPortAttr.stPoint.u32X,
            //        stChnPortAttr.stPoint.u32Y,
            //        stRgnAttr.stOsdInitParam.stSize.u32Width,
            //        stRgnAttr.stOsdInitParam.stSize.u32Height,
            //        overlay_param->pos_x,
            //        overlay_param->pos_y,
            //        overlay_param->width,
            //        overlay_param->height);
            anj_mw_osd_destroy(overlay_param);
            anj_mw_osd_create(overlay_param, NULL, PIXEL_FORMAT_I2);
        }
    }

    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(iRgnCanvasIndex, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo[%d] is null\n", iRgnCanvasIndex);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    anj_mw_osd_draw_canvas(pstRgnCanvasInfo, overlay_param);

    ST_Common_OsdCanvasUpdate(iRgnCanvasIndex);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_canvas_info_clear(overlay_param_s *overlay_param)
{
    if (overlay_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    MI_S32 u32OsdHandle = overlay_param->rgn_handle;

    if (((MI_S32)u32OsdHandle <= MI_RGN_HANDLE_NULL || u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,u32OsdHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    int iRgnCanvasIndex;
    iRgnCanvasIndex = OSD_FULL_IMAGE ? (overlay_param->stream_type * OSD_TYPE_MAX) : u32OsdHandle;

    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(iRgnCanvasIndex, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo[%d] is null\n", iRgnCanvasIndex);
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    memset((MI_U16 *)pstRgnCanvasInfo->virtAddr, 0, pstRgnCanvasInfo->stSize.u32Height * pstRgnCanvasInfo->u32Stride);

    ST_Common_OsdCanvasUpdate(iRgnCanvasIndex);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_update_rect(rect_param_s *rect_param)
{
    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    DrawRgnColor_t stColor;
    DrawPoint_t stLeftTopPt;
    DrawPoint_t stRightBotPt;
    memset(&stColor, 0, sizeof(DrawRgnColor_t));
    memset(&stLeftTopPt, 0, sizeof(DrawPoint_t));
    memset(&stRightBotPt, 0, sizeof(DrawPoint_t));

    if (rect_param == NULL)
    {
        __ERR("input is null or error\n");
        return E_MI_ERR_FAILED;
    }

    if (rect_param->bShow == 0)
    {
        return MI_SUCCESS;
    }

    MI_S32 hHandle = rect_param->rgn_handle;
    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(hHandle, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo is null\n");
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    stColor.ePixelFmt = pstRgnCanvasInfo->ePixelFmt;

    for (int i = 0; i < rect_param->s32RectCnt; i++)
    {
        stLeftTopPt.u16X = rect_param->draw_rect[i].pos_x;
        stLeftTopPt.u16Y = rect_param->draw_rect[i].pos_y;
        stRightBotPt.u16X = rect_param->draw_rect[i].pos_x + rect_param->draw_rect[i].width;
        stRightBotPt.u16Y = rect_param->draw_rect[i].pos_y + rect_param->draw_rect[i].height;
        stColor.u32Color = rect_param->draw_rect[i].u32Color;

        switch (pstRgnCanvasInfo->ePixelFmt)
        {
        case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
        {
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I4:
        {
            stLeftTopPt.u16X = ANJ_ALIGN_DOWN(stLeftTopPt.u16X, 2);
            stRightBotPt.u16X = ANJ_ALIGN_UP(stRightBotPt.u16X, 2);
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I2:
        {
            stLeftTopPt.u16X = ANJ_ALIGN_DOWN(stLeftTopPt.u16X, 4);
            stRightBotPt.u16X = ANJ_ALIGN_UP(stRightBotPt.u16X, 4);
            break;
        }
        default:
        {
            __ERR("unsupport %d pixel format!\n", pstRgnCanvasInfo->ePixelFmt);
            break;
        }
        }

        DrawRect((void *)pstRgnCanvasInfo->virtAddr, pstRgnCanvasInfo->u32Stride, stLeftTopPt, stRightBotPt, rect_param->u8BorderWidth, stColor);
    }
    ST_Common_OsdCanvasUpdate(hHandle);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_clean_rect(rect_param_s *rect_param)
{
    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    DrawRgnColor_t stColor;
    DrawPoint_t stLeftTopPt;
    DrawPoint_t stRightBotPt;
    memset(&stColor, 0, sizeof(DrawRgnColor_t));
    memset(&stLeftTopPt, 0, sizeof(DrawPoint_t));
    memset(&stRightBotPt, 0, sizeof(DrawPoint_t));

    if (rect_param == NULL)
    {
        __ERR("input is null or error\n");
        return E_MI_ERR_FAILED;
    }

    if (rect_param->bShow == 0)
    {
        return MI_SUCCESS;
    }

    MI_S32 hHandle = rect_param->rgn_handle;
    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(hHandle, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo is null\n");
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    stColor.ePixelFmt = pstRgnCanvasInfo->ePixelFmt;

    for (int i = 0; i < rect_param->s32RectCnt; i++)
    {
        stLeftTopPt.u16X = rect_param->draw_rect[i].pos_x;
        stLeftTopPt.u16Y = rect_param->draw_rect[i].pos_y;
        stRightBotPt.u16X = rect_param->draw_rect[i].pos_x + rect_param->draw_rect[i].width;
        stRightBotPt.u16Y = rect_param->draw_rect[i].pos_y + rect_param->draw_rect[i].height;

        switch (pstRgnCanvasInfo->ePixelFmt)
        {
        case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
        {
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I4:
        {
            stLeftTopPt.u16X = ANJ_ALIGN_DOWN(stLeftTopPt.u16X, 2);
            stRightBotPt.u16X = ANJ_ALIGN_UP(stRightBotPt.u16X, 2);
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I2:
        {
            stLeftTopPt.u16X = ANJ_ALIGN_DOWN(stLeftTopPt.u16X, 4);
            stRightBotPt.u16X = ANJ_ALIGN_UP(stRightBotPt.u16X, 4);
            break;
        }
        default:
        {
            __ERR("unsupport %d pixel format!\n", pstRgnCanvasInfo->ePixelFmt);
            break;
        }
        }

        DrawRect((void *)pstRgnCanvasInfo->virtAddr, pstRgnCanvasInfo->u32Stride, stLeftTopPt, stRightBotPt, rect_param->u8BorderWidth, stColor);
    }
    ST_Common_OsdCanvasUpdate(hHandle);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_update_line(line_param_s *line_param)
{
    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    DrawRgnColor_t stColor;
    DrawPoint_t stStartPt;
    DrawPoint_t stEndPt;
    memset(&stColor, 0, sizeof(DrawRgnColor_t));
    memset(&stStartPt, 0, sizeof(DrawPoint_t));
    memset(&stEndPt, 0, sizeof(DrawPoint_t));

    if (line_param == NULL)
    {
        __ERR("input is null or error\n");
        return E_MI_ERR_FAILED;
    }

    if (line_param->bShow == 0)
    {
        return MI_SUCCESS;
    }

    MI_S32 hHandle = line_param->rgn_handle;
    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(hHandle, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo is null\n");
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    stColor.ePixelFmt = pstRgnCanvasInfo->ePixelFmt;

    for (int i = 0; i < line_param->s32LineCnt; i++)
    {
        stColor.u32Color = line_param->u32Color[i];
        stStartPt.u16X = line_param->stPoint[i].pos_x;
        stStartPt.u16Y = line_param->stPoint[i].pos_y;
        stEndPt.u16X = line_param->enPoint[i].pos_x;
        stEndPt.u16Y = line_param->enPoint[i].pos_y;

        switch (pstRgnCanvasInfo->ePixelFmt)
        {
        case E_MI_RGN_PIXEL_FORMAT_ARGB1555:
        {
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I4:
        {
            stStartPt.u16X = ANJ_ALIGN_DOWN(stStartPt.u16X, 2);
            stStartPt.u16Y = ANJ_ALIGN_DOWN(stStartPt.u16Y, 2);
            stEndPt.u16X = ANJ_ALIGN_DOWN(stEndPt.u16X, 2);
            stEndPt.u16Y = ANJ_ALIGN_DOWN(stEndPt.u16Y, 2);
            break;
        }
        case E_MI_RGN_PIXEL_FORMAT_I2:
        {
            stStartPt.u16X = ANJ_ALIGN_DOWN(stStartPt.u16X, 4);
            stStartPt.u16Y = ANJ_ALIGN_DOWN(stStartPt.u16Y, 4);
            stEndPt.u16X = ANJ_ALIGN_DOWN(stEndPt.u16X, 4);
            stEndPt.u16Y = ANJ_ALIGN_DOWN(stEndPt.u16Y, 4);
            break;
        }
        default:
        {
            __ERR("unsupport %d pixel format!\n", pstRgnCanvasInfo->ePixelFmt);
            break;
        }
        }

        DrawLine((void *)pstRgnCanvasInfo->virtAddr, pstRgnCanvasInfo->u32Stride, pstRgnCanvasInfo->stSize.u32Height, pstRgnCanvasInfo->stSize.u32Width, stStartPt, stEndPt, line_param->u8BorderWidth, stColor);
    }

    ST_Common_OsdCanvasUpdate(hHandle);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_clean_line(line_param_s *line_param)
{
    MI_RGN_CanvasInfo_t *pstRgnCanvasInfo = NULL;
    DrawRgnColor_t stColor;
    DrawPoint_t stStartPt;
    DrawPoint_t stEndPt;
    memset(&stColor, 0, sizeof(DrawRgnColor_t));
    memset(&stStartPt, 0, sizeof(DrawPoint_t));
    memset(&stEndPt, 0, sizeof(DrawPoint_t));

    if (line_param == NULL)
    {
        __ERR("input is null or error\n");
        return E_MI_ERR_FAILED;
    }

    if (line_param->bShow == 0)
    {
        return MI_SUCCESS;
    }

    MI_S32 hHandle = line_param->rgn_handle;
    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    anj_mutex_lock(&s_stMutexOsdUptState);
    ST_Common_OsdCanvasGet(hHandle, &pstRgnCanvasInfo);
    if (pstRgnCanvasInfo == NULL)
    {
        __ERR("pstRgnCanvasInfo is null\n");
        anj_mutex_unlock(&s_stMutexOsdUptState);
        return E_MI_ERR_FAILED;
    }

    memset((MI_U16 *)pstRgnCanvasInfo->virtAddr, 0, pstRgnCanvasInfo->stSize.u32Height * pstRgnCanvasInfo->u32Stride);

    ST_Common_OsdCanvasUpdate(hHandle);
    anj_mutex_unlock(&s_stMutexOsdUptState);

    return MI_SUCCESS;
}

int anj_mw_osd_draw_overlay_create(int stream_type, int stream_width, int stream_height, int rgn_handle)
{
    MI_S32 s32Ret = MI_SUCCESS;
    MI_RGN_ChnPort_t m_stRgnChnPort;
    MI_RGN_Size_t m_stRgnSize;
    MI_RGN_Point_t m_stRgnPoint;
    overlay_param_s stOverlayParam;
    char cmd[128] = {0};

    if (OSD_FULL_IMAGE)
    {
        return MI_SUCCESS;
    }

    if (stream_width <= 0 || stream_height <= 0)
    {
        return MI_SUCCESS;
    }

    MI_S32 u32OsdHandle = rgn_handle;
    if (((MI_S32)u32OsdHandle <= MI_RGN_HANDLE_NULL) || (u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    memset(&m_stRgnChnPort, 0, sizeof(m_stRgnChnPort));
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_VENC;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = stream_type;
    m_stRgnChnPort.s32PortId = 0;

    m_stRgnSize.u32Width = ANJ_ALIGN_UP((MI_U32)stream_width, 8);
    m_stRgnSize.u32Height = ANJ_ALIGN_UP((MI_U32)stream_height, 8);
    m_stRgnPoint.u32X = 0;
    m_stRgnPoint.u32Y = 0;

    MI_RGN_Attr_t stRgnAttr;
    memset(&stRgnAttr, 0x00, sizeof(MI_RGN_Attr_t));
    stRgnAttr.eType = E_MI_RGN_TYPE_OSD;
    stRgnAttr.stOsdInitParam.ePixelFmt = E_MI_RGN_PIXEL_FORMAT_I2;
    stRgnAttr.stOsdInitParam.stSize.u32Width = m_stRgnSize.u32Width;
    stRgnAttr.stOsdInitParam.stSize.u32Height = m_stRgnSize.u32Height;

    MI_RGN_ChnPortParam_t stRgnChnPortParam;
    memset(&stRgnChnPortParam, 0x00, sizeof(MI_RGN_ChnPortParam_t));
    stRgnChnPortParam.bShow = TRUE;
    stRgnChnPortParam.stPoint.u32X = m_stRgnPoint.u32X;
    stRgnChnPortParam.stPoint.u32Y = m_stRgnPoint.u32Y;
    stRgnChnPortParam.unPara.stOsdChnPort.u32Layer = (MI_U32)u32OsdHandle;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.eAlphaMode = E_MI_RGN_PIXEL_ALPHA;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8BgAlpha = 0x0;
    stRgnChnPortParam.unPara.stOsdChnPort.stOsdAlphaAttr.stAlphaPara.stArgb1555Alpha.u8FgAlpha = 0xAA;

    s32Ret = ST_Common_OsdCreate(u32OsdHandle, &stRgnAttr, &m_stRgnChnPort, &stRgnChnPortParam);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("ST_Common_OsdCreate(draw) error(0x%X) h=%d\n", s32Ret, u32OsdHandle);
        return s32Ret;
    }

    snprintf(cmd, sizeof(cmd), "echo setMaxCanvasForHandle %d %d >  /proc/mi_modules/mi_rgn/mi_rgn0", u32OsdHandle,
             MAX_OSD_HANDLE_CANVAS);
    anj_mw_system(cmd);

    memset(&stOverlayParam, 0, sizeof(stOverlayParam));
    stOverlayParam.rgn_handle = rgn_handle;
    stOverlayParam.stream_type = stream_type;
    s32Ret = anj_mw_osd_canvas_info_clear(&stOverlayParam);
    if (s32Ret != MI_SUCCESS)
    {
        __ERR("anj_mw_osd_canvas_info_clear(draw) error(0x%X)\n", s32Ret);
    }

    __INFO("draw overlay handle:%d stream:%d %ux%u create sucess!\n", u32OsdHandle, stream_type, m_stRgnSize.u32Width,
           m_stRgnSize.u32Height);
    return s32Ret;
}

int anj_mw_osd_draw_overlay_destroy(int stream_type, int rgn_handle)
{
    MI_S32 s32Ret = MI_SUCCESS;
    MI_RGN_ChnPort_t m_stRgnChnPort;

    if (OSD_FULL_IMAGE)
    {
        return MI_SUCCESS;
    }

    MI_S32 u32OsdHandle = rgn_handle;
    if ((u32OsdHandle <= MI_RGN_HANDLE_NULL || u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        return MI_SUCCESS;
    }

    memset(&m_stRgnChnPort, 0, sizeof(m_stRgnChnPort));
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_VENC;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = stream_type;
    m_stRgnChnPort.s32PortId = 0;

    s32Ret = ST_Common_OsdDestory(u32OsdHandle, &m_stRgnChnPort);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("ST_Common_OsdDestory(draw) error(0x%X), hdl=%d\n", s32Ret, u32OsdHandle);
        return s32Ret;
    }

    return s32Ret;
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

int anj_mw_osd_cover_create(cover_param_s *cover_param)
{
    MI_S32 s32Ret = MI_SUCCESS;
    MI_RGN_ChnPort_t m_stRgnChnPort;
    MI_RGN_Size_t m_stRgnSize;
    MI_RGN_Point_t m_stRgnPoint;

    if (cover_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    MI_S32 u32OsdHandle = cover_param->rgn_handle;
    if (((MI_S32)u32OsdHandle <= MI_RGN_HANDLE_NULL) || (u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    memset(&m_stRgnChnPort, 0, sizeof(m_stRgnChnPort));
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_SCL;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = cover_param->stream_type;
    m_stRgnChnPort.s32PortId = 0;

    m_stRgnSize.u32Width = cover_param->cover_rect.width;
    m_stRgnSize.u32Height = cover_param->cover_rect.height;
    m_stRgnPoint.u32X = cover_param->cover_rect.pos_x;
    m_stRgnPoint.u32Y = cover_param->cover_rect.pos_y;

    if (m_stRgnSize.u32Width * m_stRgnSize.u32Height > 0)
    {
        MI_RGN_Attr_t stRgnAttr;
        memset(&stRgnAttr, 0x00, sizeof(MI_RGN_Attr_t));
        stRgnAttr.eType = E_MI_RGN_TYPE_COVER;
        stRgnAttr.stOsdInitParam.ePixelFmt = E_MI_RGN_PIXEL_FORMAT_ARGB8888;
        stRgnAttr.stOsdInitParam.stSize.u32Width = m_stRgnSize.u32Width;
        stRgnAttr.stOsdInitParam.stSize.u32Height = m_stRgnSize.u32Height;

        MI_RGN_ChnPortParam_t stRgnChnPortParam;
        memset(&stRgnChnPortParam, 0x00, sizeof(MI_RGN_ChnPortParam_t));
        stRgnChnPortParam.bShow = TRUE;
        stRgnChnPortParam.stPoint.u32X = m_stRgnPoint.u32X;
        stRgnChnPortParam.stPoint.u32Y = m_stRgnPoint.u32Y;
        stRgnChnPortParam.unPara.stCoverChnPort.u32Layer = 0;
        stRgnChnPortParam.unPara.stCoverChnPort.stSize.u32Width = m_stRgnSize.u32Width;
        stRgnChnPortParam.unPara.stCoverChnPort.stSize.u32Height = m_stRgnSize.u32Height;
        stRgnChnPortParam.unPara.stCoverChnPort.u32Color = cover_param->cover_rect.u32Color;

        s32Ret = ST_Common_OsdCreate(u32OsdHandle, &stRgnAttr, &m_stRgnChnPort, &stRgnChnPortParam);
        if (s32Ret != MI_SUCCESS)
        {
            __ERR("ST_Common_OsdCreate() error(0x%X)\n", s32Ret);
        }
        usleep(100 * 1000);
        __INFO("cover handle:%d xy:%d %d wd:%d %d create sucess!\n",
               u32OsdHandle, m_stRgnPoint.u32X, m_stRgnPoint.u32Y, m_stRgnSize.u32Width, m_stRgnSize.u32Height);
    }

    return s32Ret;
}

int anj_mw_osd_cover_destroy(cover_param_s *cover_param)
{
    MI_S32 s32Ret = MI_SUCCESS;

    if (cover_param == NULL)
    {
        __ERR("input is null\n");
        return E_MI_ERR_FAILED;
    }

    MI_S32 u32OsdHandle = cover_param->rgn_handle;
    if ((u32OsdHandle <= MI_RGN_HANDLE_NULL || u32OsdHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", u32OsdHandle);
        return E_MI_ERR_FAILED;
    }

    MI_RGN_ChnPort_t m_stRgnChnPort = {0};
    m_stRgnChnPort.eModId = E_MI_MODULE_ID_SCL;
    m_stRgnChnPort.s32DevId = 0;
    m_stRgnChnPort.s32ChnId = cover_param->stream_type;
    m_stRgnChnPort.s32PortId = 0;

    s32Ret = ST_Common_OsdDestory(u32OsdHandle, &m_stRgnChnPort);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("ST_Common_OsdDestory error(0x%X), hdl=%d\n", s32Ret, u32OsdHandle);
        return s32Ret;
    }

    return s32Ret;
}

