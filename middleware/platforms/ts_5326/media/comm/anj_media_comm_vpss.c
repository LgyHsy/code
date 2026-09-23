
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include "anj_mw_media_common.h"
#include "anj_mw_log.h"

/*****************************************************************************
 * function : start vpss grp.
 *****************************************************************************/
TS_S32 TS_COMMON_VPSS_Start(VPSS_GRP VpssGrp, VPSS_GRP_ATTR_S *pstVpssGrpAttr, VPSS_CHN_ATTR_S *pastVpssChnAttr,
                            TS_S32 chnlNum)
{
    VPSS_CHN VpssChn;
    TS_S32 s32Ret;
    TS_S32 j;

    s32Ret = TS_MPI_VPSS_CreateGrp(VpssGrp, pstVpssGrpAttr);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return TS_FAILURE;
    }

    __INFO("vpss grp[%d] : chnlNum=%d\n", VpssGrp, chnlNum);

    for (j = 0; j < chnlNum; j++)
    {
        VpssChn = j;
        s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &pastVpssChnAttr[VpssChn]);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x\n", VpssGrp, j, s32Ret);
            return TS_FAILURE;
        }
        __INFO("TS_MPI_VPSS_SetChnAttrr(grp:%d, chn:%d) success\n", VpssGrp, j);

        s32Ret = TS_MPI_VPSS_EnableChn(VpssGrp, VpssChn);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_EnableChn(grp:%d, chn:%d) success\n", VpssGrp, j);
            return TS_FAILURE;
        }
        __INFO("TS_MPI_VPSS_EnableChn(grp:%d, chn:%d) success\n", VpssGrp, j);
    }

    __INFO("is going to TS_MPI_VPSS_StartGrp [%d]\n", VpssGrp);
    s32Ret = TS_MPI_VPSS_StartGrp(VpssGrp);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

/*****************************************************************************
 * function : start vpss grp.
 *****************************************************************************/
TS_S32 TS_COMMON_VPSS_Start_Index(VPSS_GRP VpssGrp, VPSS_GRP_ATTR_S *pstVpssGrpAttr, VPSS_CHN_ATTR_S *pastVpssChnAttr,
                                  TS_S32 chnlNum, TS_S32 index)
{
    VPSS_CHN VpssChn;
    TS_S32 s32Ret;
    TS_S32 j;

    s32Ret = TS_MPI_VPSS_CreateGrp(VpssGrp, pstVpssGrpAttr);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return TS_FAILURE;
    }

    __ERR("vpss grp[%d] : chnlNum=%d\n", VpssGrp, chnlNum);

    for (j = index; j < chnlNum; j++)
    {
        VpssChn = j;
        s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &pastVpssChnAttr[VpssChn]);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x\n", VpssGrp, j, s32Ret);
            return TS_FAILURE;
        }
        __ERR("TS_MPI_VPSS_SetChnAttrr(grp:%d, chn:%d) success\n", VpssGrp, j);

        s32Ret = TS_MPI_VPSS_EnableChn(VpssGrp, VpssChn);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_EnableChn(grp:%d, chn:%d) success\n", VpssGrp, j);
            return TS_FAILURE;
        }
        __ERR("TS_MPI_VPSS_EnableChn(grp:%d, chn:%d) success\n", VpssGrp, j);
    }

    __ERR("is going to TS_MPI_VPSS_StartGrp [%d]\n", VpssGrp);
    s32Ret = TS_MPI_VPSS_StartGrp(VpssGrp);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

/*****************************************************************************
 * function : stop vpss grp
 *****************************************************************************/
TS_S32 TS_COMMON_VPSS_Stop(VPSS_GRP VpssGrp, TS_S32 chnlNum)
{
    TS_S32 j;
    TS_S32 s32Ret = TS_SUCCESS;

    for (j = 0; j < chnlNum; j++)
    {
        VPSS_CHN VpssChn = j;
        s32Ret = TS_MPI_VPSS_DisableChn(VpssGrp, VpssChn);

        if (s32Ret != TS_SUCCESS)
        {
            __ERR("failed with %#x!\n", s32Ret);
            return TS_FAILURE;
        }
        __INFO("TS_MPI_VPSS_DisableChn [%d:%d] success\n", VpssGrp, VpssChn);
    }

    s32Ret = TS_MPI_VPSS_StopGrp(VpssGrp);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    __INFO("TS_MPI_VPSS_StopGrp [%d] success\n", VpssGrp);

    s32Ret = TS_MPI_VPSS_DestroyGrp(VpssGrp);

    if (s32Ret != TS_SUCCESS)
    {
        __ERR("failed with %#x!\n", s32Ret);
        return TS_FAILURE;
    }
    __INFO("TS_MPI_VPSS_DestroyGrp [%d] success\n", VpssGrp);

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_ChnCrop(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_S32 x, TS_S32 y, TS_U32 width, TS_U32 height)
{
    TS_S32 s32Ret;
    const VPSS_CROP_INFO_S stCropInfo = {TS_TRUE, VPSS_CROP_ABS_COOR,
                                         .stCropRect = {
                                             x,
                                             y,
                                             width,
                                             height,
                                         }};

    s32Ret = TS_MPI_VPSS_SetChnCrop(VpssGrp, VpssChn, &stCropInfo);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_SetChnCrop(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_GrpCrop(VPSS_GRP VpssGrp, TS_S32 x, TS_S32 y, TS_U32 width, TS_U32 height)
{
    TS_S32 s32Ret;
    const VPSS_CROP_INFO_S stCropInfo = {TS_TRUE, VPSS_CROP_ABS_COOR,
                                         .stCropRect = {
                                             x,
                                             y,
                                             width,
                                             height,
                                         }};

    s32Ret = TS_MPI_VPSS_SetGrpCrop(VpssGrp, &stCropInfo);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_SetGrpCrop(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_SetFlip(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_U32 bFlip)
{
    VPSS_CHN_ATTR_S stVpssChnAttr = {0};
    TS_S32 s32Ret;

    s32Ret = TS_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_GetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    if (stVpssChnAttr.bFlip != bFlip)
    {
        stVpssChnAttr.bFlip = bFlip;
        s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
            return TS_FAILURE;
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_SetMirror(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_U32 bMirror)
{
    VPSS_CHN_ATTR_S stVpssChnAttr = {0};
    TS_S32 s32Ret;

    s32Ret = TS_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_GetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    __ERR("TS_MPI_VPSS_GetChnAttr(grp:%d, chn:%d) memCount=%d\n", VpssGrp, VpssChn, stVpssChnAttr.u32MemCount);

    if (stVpssChnAttr.bMirror != bMirror)
    {
        stVpssChnAttr.bMirror = bMirror;
        s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
            return TS_FAILURE;
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_SetFlipMirror(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_U32 bFlip, TS_U32 bMirror)
{
    VPSS_CHN_ATTR_S stVpssChnAttr = {0};
    TS_S32 s32Ret;

    s32Ret = TS_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_GetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    if ((stVpssChnAttr.bMirror != bMirror) || (stVpssChnAttr.bFlip != bFlip))
    {
        stVpssChnAttr.bFlip = bFlip;
        stVpssChnAttr.bMirror = bMirror;
        s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
        if (s32Ret != TS_SUCCESS)
        {
            __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
            return TS_FAILURE;
        }
    }

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_SetFps(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_U32 dFps, TS_U32 sFps)
{
    VPSS_CHN_ATTR_S stVpssChnAttr = {0};
    TS_S32 s32Ret;

    s32Ret = TS_MPI_VPSS_GetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_GetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    if (dFps > sFps)
        return TS_FAILURE;

    stVpssChnAttr.stFrameRate.s32SrcFrameRate = sFps;

    stVpssChnAttr.stFrameRate.s32DstFrameRate = dFps;

    s32Ret = TS_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stVpssChnAttr);
    if (s32Ret != TS_SUCCESS)
    {
        __ERR("TS_MPI_VPSS_SetChnAttr(grp:%d, chn:%d) failed with %#x!\n", VpssGrp, VpssChn, s32Ret);
        return TS_FAILURE;
    }

    return TS_SUCCESS;
}
