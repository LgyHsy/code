#include "anj_mw_media_common.h"
#include "anj_mw_media_osd.h"

static MI_RGN_CanvasInfo_t stCanvasInfo;

typedef struct
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ColorRGB;

static void set_color(ColorRGB *p, unsigned char r, unsigned char g, unsigned char b)
{
    p->r = r;
    p->g = g;
    p->b = b;
}

// 256色调色板，256个元素都有相应的颜色值，0位置为透明色
static void get_colorI8_PaletteTable(ColorRGB *table)
{
    /*256个字节的调色板，R/G/B分别用8 8 4个字节来循环设置，用于覆盖所有颜色范围*/
    /*计算888的RGB颜色时，R G对0X20取整得到数组位置，B对0X40取整得到数组位置*/
    unsigned char color8[] = {0x01, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xE0};
    unsigned char color4[] = {0x01, 0x40, 0x80, 0xC0};

    int iIndex = 0, jIndex = 0, kIndex = 0;
    for (iIndex = 0; iIndex < 8; iIndex++)
    {
        for (jIndex = 0; jIndex < 8; jIndex++)
        {
            for (kIndex = 0; kIndex < 4; kIndex++)
            {
                int pos = ((iIndex * 8) + jIndex) * 4 + kIndex;
                table[pos].r = color8[iIndex];
                table[pos].g = color8[jIndex];
                table[pos].b = color4[kIndex];
            }
        }
    }
    table[0].r = 0;
    table[0].g = 0;
    table[0].b = 0;
}

static void get_colorI2_PaletteTable(ColorRGB table[256])
{
    /*位置0用于设置透明颜色，位置1用于设置全黑,位置255用于设置全白*/
    set_color(&table[0], 0, 0, 0);         // rgb不重要，还有一个alpha决定透明色
    set_color(&table[1], 0, 0, 0);         // 黑
    set_color(&table[2], 255, 255, 255);   // 白
    set_color(&table[3], 0, 255, 0);       // 绿
    set_color(&table[255], 255, 255, 255); // 白
}

static void get_colorI4_PaletteTable(ColorRGB table[256])
{
    get_colorI2_PaletteTable(table);
    set_color(&table[4], 255, 0, 0);      // 红
    set_color(&table[5], 0, 0, 255);      // 蓝
    set_color(&table[6], 255, 128, 0);    // 橙
    set_color(&table[7], 255, 255, 0);    // 黄
    set_color(&table[8], 0, 128, 128);    // 青
    set_color(&table[9], 128, 0, 128);    // 紫色
    set_color(&table[10], 0, 255, 255);   // 水绿
    set_color(&table[11], 0, 128, 255);   // 深绿
    set_color(&table[12], 255, 0, 255);   // 紫红
    set_color(&table[13], 128, 128, 128); // 灰
    set_color(&table[14], 0, 0, 128);     // 海军蓝
    set_color(&table[15], 128, 128, 0);   // 橄榄色
}

MI_S32 ST_Common_OsdCanvasUpdate(MI_RGN_HANDLE hHandle)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;
    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    s32Ret = MI_RGN_UpdateCanvas(0, hHandle);
    if (s32Ret != MI_RGN_OK)
    {
        __ERR("MI_RGN_UpdateCanvas fail\n");
        return s32Ret;
    }

    return s32Ret;
}

MI_S32 ST_Common_OsdCanvasGet(MI_RGN_HANDLE hHandle, MI_RGN_CanvasInfo_t **pstRgnCanvasInfo)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    if (((MI_S32)hHandle <= MI_RGN_HANDLE_NULL || hHandle >= MI_RGN_MAX_HANDLE))
    {
        __ERR("OSD handle error,hHandle=%d\n", hHandle);
        return E_MI_ERR_FAILED;
    }

    if (*pstRgnCanvasInfo == NULL)
    {
        s32Ret = MI_RGN_GetCanvasInfo(0, hHandle, &stCanvasInfo);
        if (s32Ret != MI_RGN_OK)
        {
            __ERR("MI_RGN_GetCanvasInfo error s32Ret=%x\n", s32Ret);
            return s32Ret;
        }
        *pstRgnCanvasInfo = &stCanvasInfo;
    }

    return s32Ret;
}

MI_S32 ST_Common_OsdAttrGet(MI_RGN_HANDLE hHandle, MI_RGN_Attr_t *pstRgnAttr)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    if (hHandle < 0)
    {
        __ERR("The input Rgn handle(%d) is out of range!\n", hHandle);
        s32Ret = E_MI_ERR_ILLEGAL_PARAM;
        return s32Ret;
    }

    if (NULL == pstRgnAttr)
    {
        __ERR("the input pointer is NULL!\n");
        s32Ret = E_MI_ERR_NULL_PTR;
        return s32Ret;
    }
    s32Ret = MI_RGN_GetAttr(0, hHandle, pstRgnAttr);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_GetAttr Hdl=%d error, %X\n", hHandle, s32Ret);
    }
    return s32Ret;
}

MI_S32 ST_Common_OsdDisplayAttrGet(MI_RGN_HANDLE hHandle, MI_RGN_ChnPort_t *pstChnPort, MI_RGN_ChnPortParam_t *pstChnPortAttr)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    if (hHandle < 0)
    {
        __ERR("The input Rgn handle(%d) is out of range!\n", hHandle);
        s32Ret = E_MI_ERR_ILLEGAL_PARAM;
        return s32Ret;
    }

    if (NULL == pstChnPort || NULL == pstChnPortAttr)
    {
        __ERR("the input pointer is NULL!\n");
        s32Ret = E_MI_ERR_NULL_PTR;
        return s32Ret;
    }
    s32Ret = MI_RGN_GetDisplayAttr(0, hHandle, pstChnPort, pstChnPortAttr);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_GetDisplayAttr Hdl=%d error, %X\n", hHandle, s32Ret);
    }
    return s32Ret;
}

MI_S32 ST_Common_OsdCreate(MI_RGN_HANDLE hHandle, MI_RGN_Attr_t *pstRgnAttr, MI_RGN_ChnPort_t *pstRgnChnPort, MI_RGN_ChnPortParam_t *pstRgnChnPortParam)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    if (hHandle < 0)
    {
        __ERR("The input Rgn handle(%d) is out of range!\n", hHandle);
        s32Ret = E_MI_ERR_ILLEGAL_PARAM;
        return s32Ret;
    }

    if ((NULL == pstRgnAttr) || (NULL == pstRgnChnPort) || (NULL == pstRgnChnPortParam))
    {
        __ERR("the input pointer is NULL!\n");
        s32Ret = E_MI_ERR_NULL_PTR;
        return s32Ret;
    }

    s32Ret = MI_RGN_Create(0, hHandle, pstRgnAttr);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_Create error, %X\n", s32Ret);
        printf("Hdl=%d RGN_Attr:Type=%d, Width=%4d, Heitht=%4d, fmt=%d\n", hHandle, pstRgnAttr->eType,
               pstRgnAttr->stOsdInitParam.stSize.u32Width, pstRgnAttr->stOsdInitParam.stSize.u32Height, pstRgnAttr->stOsdInitParam.ePixelFmt);
        return s32Ret;
    }

    s32Ret = MI_RGN_AttachToChn(0, hHandle, pstRgnChnPort, pstRgnChnPortParam);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_AttachToChn error, %X\n", s32Ret);
        s32Ret = MI_RGN_Destroy(0, hHandle);
        if (MI_RGN_OK != s32Ret)
            __ERR("MI_RGN_Destroy error, %X\n", s32Ret);

        __INFO("Hdl=%d RGN_Attr:Type=%d, Width=%4d, Heitht=%4d, fmt=%d\n", hHandle, pstRgnAttr->eType,
               pstRgnAttr->stOsdInitParam.stSize.u32Width, pstRgnAttr->stOsdInitParam.stSize.u32Height, pstRgnAttr->stOsdInitParam.ePixelFmt);
        __INFO(" Hdl=%d RGN:ModId=%d, DevId=%d, ChnId=%d, PortId=%d, fmt=I4\n", hHandle, pstRgnChnPort->eModId,
               pstRgnChnPort->s32DevId, pstRgnChnPort->s32ChnId, pstRgnChnPort->s32PortId);
        __INFO("Hdl=%d canvas:x=%d, y=%d,\n", hHandle,
               pstRgnChnPortParam->stPoint.u32X, pstRgnChnPortParam->stPoint.u32Y);
        return s32Ret;
    }

    s32Ret = MI_RGN_OK;
    return s32Ret;
}

MI_S32 ST_Common_OsdDestory(MI_RGN_HANDLE hHandle, MI_RGN_ChnPort_t *pstRgnChnPort)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    if (hHandle < 0)
    {
        __ERR("The input Rgn handle(%d) is out of range!\n", hHandle);
        s32Ret = E_MI_ERR_ILLEGAL_PARAM;
        return s32Ret;
    }

    if (NULL == pstRgnChnPort)
    {
        __ERR("the input pointer is NULL!\n");
        s32Ret = E_MI_ERR_NULL_PTR;
        return s32Ret;
    }

    s32Ret = MI_RGN_DetachFromChn(0, hHandle, pstRgnChnPort);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_DetachFromChn error(0x%X), hdl=%d\n", s32Ret, hHandle);
        return s32Ret;
    }

    s32Ret = MI_RGN_Destroy(0, hHandle);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_Destroy error(0x%X), hdl=%d\n", s32Ret, hHandle);
        return s32Ret;
    }

    return s32Ret;
}

MI_S32 ST_Common_OsdRgnInit(int pixel_fmt)
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;
    MI_RGN_InitParam_t stInitParam;
    memset(&stInitParam, 0, sizeof(stInitParam));
    MI_RGN_PaletteTable_t stPaletteTable;
    memset(&stPaletteTable, 0, sizeof(stPaletteTable));
    ColorRGB table[256] = {0};
    if (pixel_fmt == PIXEL_FORMAT_I2)
    {
        get_colorI2_PaletteTable(table);
    }
    else if (pixel_fmt == PIXEL_FORMAT_I4)
    {
        get_colorI4_PaletteTable(table);
    }
    else
    {
        get_colorI8_PaletteTable(table);
    }

    /*位置0用于设置透明颜色，位置1用于设置全黑,位置255用于设置全白*/
    stPaletteTable.astElement[0].u8Alpha = 0;
    stPaletteTable.astElement[0].u8Red = 0;
    stPaletteTable.astElement[0].u8Green = 0;
    stPaletteTable.astElement[0].u8Blue = 0;

    stPaletteTable.astElement[1].u8Alpha = 255 * (90) / 100;
    stPaletteTable.astElement[1].u8Red = 0;
    stPaletteTable.astElement[1].u8Green = 0;
    stPaletteTable.astElement[1].u8Blue = 0;

    for (int iIndex = 2; iIndex < 256; iIndex++)
    {
        stPaletteTable.astElement[iIndex].u8Alpha = 255 * (90) / 100;
        stPaletteTable.astElement[iIndex].u8Red = table[iIndex].r;
        stPaletteTable.astElement[iIndex].u8Green = table[iIndex].g;
        stPaletteTable.astElement[iIndex].u8Blue = table[iIndex].b;
    }

    stInitParam.pstPaletteTable = &stPaletteTable;
    s32Ret = MI_RGN_InitDev(0, &stInitParam);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_InitDev error(%X)\n", s32Ret);
    }
    return s32Ret;
}

MI_S32 ST_Common_OsdRgnUnInit()
{
    MI_S32 s32Ret = E_MI_ERR_FAILED;

    s32Ret = MI_RGN_DeInitDev(0);
    if (MI_RGN_OK != s32Ret)
    {
        __ERR("MI_RGN_DeInitDev error(%X)\n", s32Ret);
    }
    return s32Ret;
}
