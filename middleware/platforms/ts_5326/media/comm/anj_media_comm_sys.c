
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
#include <sys/mman.h>
#include <stdatomic.h>

#include "anj_mw_media_common.h"

char *TS_Common_SysMmap(unsigned long long u64PhyAddr, unsigned int mapsize)
{
    void *pMappedAddr = NULL;
    TS_COMMON_SYS_Init();
    pMappedAddr = TS_MPI_SYS_Mmap(u64PhyAddr, mapsize);
    return pMappedAddr;
}

void TS_Common_SysMunmap(void *pVirtualAddress, unsigned int mapsize)
{
    TS_MPI_SYS_Munmap(pVirtualAddress, mapsize);
}

TS_S32 TS_Common_SysMmz_Alloc(unsigned char *pstMMAHeapName, unsigned int u32BlkSize, unsigned long long *phyAddr, unsigned long long *virAddr)
{
    TS_S32 s32Ret = TS_FAILURE;
    TS_COMMON_SYS_Init();

    s32Ret = TS_MPI_SYS_MmzAlloc(phyAddr, (TS_VOID **)&virAddr, NULL, NULL, u32BlkSize);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_MmzAlloc failed!\n");
    }

    return s32Ret;
}

void TS_Common_SysMmz_Free(unsigned long long phyAddr, unsigned long long *virAddr)
{
    TS_MPI_SYS_MmzFree(phyAddr, virAddr);
}

void TS_COMMON_SYS_ShowVersion()
{
    MPP_VERSION_S stVersion = {0};
    TS_U64 u64ChipId;
    TS_U32 u32Pid;
    TS_U32 u32Partid;

    TS_MPI_SYS_GetVersion(&stVersion);
    TS_MPI_SYS_GetPid(&u32Pid);
    TS_MPI_SYS_GetChipId(&u64ChipId);
    TS_MPI_SYS_GetPartid(&u32Partid);

    __INFO("[%s]: is working now. chipId=%llx, pid=%x, partId=%x\n", stVersion.aVersion, u64ChipId,
           u32Pid, u32Partid);
}

/******************************************************************************
 * function : noc bandwidth limit via sysfs
 ******************************************************************************/
TS_S32 TS_COMMON_SYS_BwLimitInit(TS_Common_ViAttr_t *pstViAttr)
{
    TS_BOOL bWdr = TS_FALSE;

    if (pstViAttr == NULL)
    {
        __ERR("pstViAttr is NULL\n");
        return TS_FAILURE;
    }

    bWdr = (pstViAttr->astViInfo[0].stPipeInfo.enWdrMode != WDR_MODE_NONE) ? TS_TRUE : TS_FALSE;
    if (bWdr)
    {
        system("devmem 0xF0620114 32 0x20");
        system("devmem 0xf0620094 32 0x40");
        system("echo \"isp 1600 3 4 5\" > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
        system("echo \"vpe 800 1 4 2\" > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
        system("echo \"vpu 800 1 2 2\" > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
        system("echo \"rne 200 1 0 0\" > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
        system("echo \"cpu 2 2\" > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
    }
    else
    {
        system("devmem 0xF0620114 32 0x40");
        system("devmem 0xf0620094 32 0x80");
        if (pstViAttr->astViInfo[0].stPipeInfo.width == 2592)
        {
            system("echo isp 1200 3 4 5 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo vpe 400 3 2 4 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo vpu 800 1 2 2 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo rne 1000 1 0 0 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo ebd 400 1 3 2 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo cpu 2 2 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");   
        }
        else
        {
            system("devmem 0xF038A0F0 32 0x404251F");
            system("echo isp 800 3 4 5 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo vpe 800 3 3 4 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo vpu 1200 1 2 2 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo rne 1600 1 0 0 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo ebd 400 1 4 2 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");
            system("echo cpu 2 3 > /sys/class/bandwidth/ts_bw_dev/noc_bw_limit");   
        }
    }

    return TS_SUCCESS;
}

/******************************************************************************
 * function : vb init & MPI system init
 ******************************************************************************/
TS_S32 TS_COMMON_SYS_Init()
{
    TS_S32 s32Ret = TS_FAILURE;

    TS_MPI_SYS_Exit();
    TS_MPI_VB_Exit();

    VB_CONFIG_S stVbConfig = {0};

    // 只是为了兼容海思的接口 实际vb底层sdk自己会去计算
    stVbConfig.u32MaxPoolCnt = 1;
    stVbConfig.astCommPool[0].u64BlkSize = 1024;
    stVbConfig.astCommPool[0].u32BlkCnt = 1;

    s32Ret = TS_MPI_VB_SetConfig(&stVbConfig);

    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VB_SetConf failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_VB_Init();

    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VB_Init failed!\n");
        return TS_FAILURE;
    }

    s32Ret = TS_MPI_SYS_Init();
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_Init failed!\n");
        TS_MPI_VB_Exit();
        return TS_FAILURE;
    }

    LOG_LEVEL_CONF_S stConf = {0};
    for (int i = TS_ID_CMPI; i <= TS_ID_BUTT; i++)
    {
        stConf.enModId = i;
        TS_MPI_LOG_GetLevelConf(&stConf);
        stConf.s32Level = TS_DBG_ERR;
        TS_MPI_LOG_SetLevelConf(&stConf);
    }
    
    return s32Ret;
}

TS_S32 TS_COMMON_MMZ_Init()
{
    return TS_SUCCESS;
    TS_S32 s32Ret = TS_FAILURE;

    s32Ret = TS_MPI_SYS_SetMmzcCfg(MMZC_ISP_MEM, -1, -1, 8644);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_SetMmzcCfg isp_mem failed!\n");
        return s32Ret;
    }

    s32Ret = TS_MPI_SYS_SetMmzcCfg(MMZC_VPSS, -1, -1, 5400);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_SetMmzcCfg vpss failed!\n");
        return s32Ret;
    }

    s32Ret = TS_MPI_SYS_SetMmzcCfg(MMZC_VENC, -1, -1, 16220);
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_SetMmzcCfg venc failed!\n");
        return s32Ret;
    }

    __INFO("TS_COMMON_MMZ_Init success\n");
    return s32Ret;
}

/******************************************************************************
 * function : vb exit & MPI system exit
 ******************************************************************************/
TS_VOID TS_COMMON_SYS_Exit(void)
{
    TS_S32 s32Ret = TS_FAILURE;

    /*TS_COMMON_VO_Exit();*/ /* only for debug */
    s32Ret = TS_MPI_SYS_Exit();
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_SYS_Exit faild with %#x\n", s32Ret);
    }
    else
    {
        __ERR("TS_MPI_SYS_Exit success\n");
    }

    // TS_MPI_VB_ExitModCommPool(VB_UID_VDEC);
    s32Ret = TS_MPI_VB_Exit();
    if (TS_SUCCESS != s32Ret)
    {
        __ERR("TS_MPI_VB_Exit failed. ret %#x\n", s32Ret);
    }
    else
    {
        __ERR("TS_MPI_VB_Exit success\n");
    }

    return;
}

TS_S32 TS_COMMON_VB_Save2File(VIDEO_FRAME_INFO_S *pVideoFrame, TS_CHAR *pPath, TS_S32 fileIdx)
{
    TS_CHAR fileName[128];
    FILE *fd = NULL;
    int bytes = 0;

    if (!pVideoFrame)
    {
        __ERR("NULL ptr \n");
        return TS_FAILURE;
    }

    __ERR("frame info : wh=[%d,%d], stride=[%d,%d], virAddr=[%llx,%llx], [%llx,%llx]\n",
          pVideoFrame->stVFrame.u32Width, pVideoFrame->stVFrame.u32Height, pVideoFrame->stVFrame.u32Stride[0],
          pVideoFrame->stVFrame.u32Stride[1], pVideoFrame->stVFrame.u64VirAddr[0],
          pVideoFrame->stVFrame.u64VirAddr[1], pVideoFrame->stVFrame.u64PhyAddr[0],
          pVideoFrame->stVFrame.u64PhyAddr[1]);

    if (fileIdx >= 0)
    {
        if (pPath)
        {
            sprintf(fileName, "%s/frame_%04d_%d_%d.yuv", pPath, fileIdx, pVideoFrame->stVFrame.u32Width,
                    pVideoFrame->stVFrame.u32Height);
        }
        else
        {
            sprintf(fileName, "./frame_%04d_%d_%d.yuv", fileIdx, pVideoFrame->stVFrame.u32Width,
                    pVideoFrame->stVFrame.u32Height);
        }
    }
    else
    {
        if (pPath)
        {
            sprintf(fileName, "%s/frame_%d_%d.yuv", pPath, pVideoFrame->stVFrame.u32Width,
                    pVideoFrame->stVFrame.u32Height);
        }
        else
        {
            sprintf(fileName, "./frame_%d_%d.yuv", pVideoFrame->stVFrame.u32Width, pVideoFrame->stVFrame.u32Height);
        }
    }

    fd = fopen(fileName, "w+");
    if (!fd)
    {
        __ERR("unable to create debug file.");
        return TS_FAILURE;
    }

    ts_char *dmaStr = mmap(NULL, pVideoFrame->stVFrame.s32Size, PROT_READ, MAP_SHARED, pVideoFrame->stVFrame.s32Fd, 0);

    if (dmaStr && dmaStr != MAP_FAILED)
    {
        bytes = fwrite(dmaStr, pVideoFrame->stVFrame.s32Size, 1, fd);
        if (bytes <= 0)
        {
            __ERR("write faild with %d, errString is %s\n", bytes, strerror(errno));
        }
        munmap(dmaStr, pVideoFrame->stVFrame.s32Size);
    }

    fclose(fd);

    __ERR("save %s success, size=%d\n", fileName, bytes);

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_Bind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp)
{
    __ERR("TS_MPI_SYS_Bind not support ViPipe:%d ViChn:%d VpssGrp:%d\n", ViPipe, ViChn, VpssGrp);
    return TS_FAILURE;
}

TS_S32 TS_COMMON_VI_UnBind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp)
{
    __ERR("TS_MPI_SYS_UnBind not support ViPipe:%d ViChn:%d VpssGrp:%d\n", ViPipe, ViChn, VpssGrp);
    return TS_FAILURE;
}

TS_S32 TS_COMMON_VI_Bind_VENC(VI_PIPE ViPipe, VI_CHN ViChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VI;
    stSrcChn.s32DevId = ViPipe;
    stSrcChn.s32ChnId = ViChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_UnBind_VENC(VI_PIPE ViPipe, VI_CHN ViChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VI;
    stSrcChn.s32DevId = ViPipe;
    stSrcChn.s32ChnId = ViChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_Bind_AVS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, AVS_GRP AvsGrp, AVS_PIPE AvsPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_AVS;
    stDestChn.s32DevId = AvsGrp;
    stDestChn.s32ChnId = AvsPipe;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_UnBind_AVS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, AVS_GRP AvsGrp, AVS_PIPE AvsPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_AVS;
    stDestChn.s32DevId = AvsGrp;
    stDestChn.s32ChnId = AvsPipe;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_Bind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VI;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VI_UnBind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VI;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_Bind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_UnBind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_CPM_Bind_VENC(CPM_GRP CpmGrp, CPM_CHN CpmChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_CPM;
    stSrcChn.s32DevId = CpmGrp;
    stSrcChn.s32ChnId = CpmChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_CPM_UnBind_VENC(CPM_GRP CpmGrp, CPM_CHN CpmChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_CPM;
    stSrcChn.s32DevId = CpmGrp;
    stSrcChn.s32ChnId = CpmChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_CPM_Bind_VO(CPM_GRP CpmGrp, CPM_CHN CpmChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_CPM;
    stSrcChn.s32DevId = CpmGrp;
    stSrcChn.s32ChnId = CpmChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_CPM_UnBind_VO(CPM_GRP CpmGrp, CPM_CHN CpmChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_CPM;
    stSrcChn.s32DevId = CpmGrp;
    stSrcChn.s32ChnId = CpmChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_Bind_VO(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_Bind_VPSS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VPSS_GRP VpssGrpDst)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_VPSS;
    stDestChn.s32DevId = VpssGrpDst;
    stDestChn.s32ChnId = 0;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_UnBind_VO(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_Bind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VPSS_UnBind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = TS_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    __ERR("TS_MPI_SYS_UnBind success! VpssGrp:%d VpssChn:%d VencChn:%d\n", VpssGrp, VpssChn, VencChn);

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VDEC_Bind_VPSS(VDEC_CHN VdecChn, VPSS_GRP VpssGrp)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_VPSS;
    stDestChn.s32DevId = VpssGrp;
    stDestChn.s32ChnId = 0;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VDEC_UnBind_VPSS(VDEC_CHN VdecChn, VPSS_GRP VpssGrp)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_VPSS;
    stDestChn.s32DevId = VpssGrp;
    stDestChn.s32ChnId = 0;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VO_Bind_VO(VO_LAYER SrcVoLayer, VO_CHN SrcVoChn, VO_LAYER DstVoLayer, VO_CHN DstVoChn)
{
    MPP_CHN_S stSrcChn, stDestChn;
    stSrcChn.enModId = TS_ID_VO;
    stSrcChn.s32DevId = SrcVoLayer;
    stSrcChn.s32ChnId = SrcVoChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = DstVoLayer;
    stDestChn.s32ChnId = DstVoChn;

    return TS_MPI_SYS_Bind(&stSrcChn, &stDestChn);
}

TS_S32 TS_COMMON_VO_UnBind_VO(VO_LAYER DstVoLayer, VO_CHN DstVoChn)
{
    MPP_CHN_S stDestChn;
    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = DstVoLayer;
    stDestChn.s32ChnId = DstVoChn;

    return TS_MPI_SYS_UnBind(NULL, &stDestChn);
}

TS_S32 TS_COMMON_VDEC_Bind_VO(VDEC_CHN VdecChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VDEC_UnBind_VO(VDEC_CHN VdecChn, VO_LAYER VoLayer, VO_CHN VoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_VO;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VDEC_Bind_CPM(VDEC_CHN VdecChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_VDEC_UnBind_CPM(VDEC_CHN VdecChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_VDEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = VdecChn;

    stDestChn.enModId = TS_ID_CPM;
    stDestChn.s32DevId = CpmGrp;
    stDestChn.s32ChnId = CpmPipe;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_AI_Bind_AO(AI_CHN AiChn, AO_CHN AoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_AI;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AiChn;

    stDestChn.enModId = TS_ID_AO;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AoChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_AI_UnBind_AO(AI_CHN AiChn, AO_CHN AoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_AI;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AiChn;

    stDestChn.enModId = TS_ID_AO;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AoChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_AI_Bind_AENC(AI_CHN AiChn, AENC_CHN AencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_AI;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AiChn;

    stDestChn.enModId = TS_ID_AENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AencChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_AI_UnBind_AENC(AI_CHN AiChn, AENC_CHN AencChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_AI;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AiChn;

    stDestChn.enModId = TS_ID_AENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AencChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_ADEC_Bind_AO(ADEC_CHN AdChn, AUDIO_DEV AoDev, AO_CHN AoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_ADEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AdChn;

    stDestChn.enModId = TS_ID_AO;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AoChn;

    STCHECKRESULT(TS_MPI_SYS_Bind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}

TS_S32 TS_COMMON_ADEC_UnBind_AO(ADEC_CHN AdChn, AUDIO_DEV AoDev, AO_CHN AoChn)
{
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = TS_ID_ADEC;
    stSrcChn.s32DevId = 0;
    stSrcChn.s32ChnId = AdChn;

    stDestChn.enModId = TS_ID_AO;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = AoChn;

    STCHECKRESULT(TS_MPI_SYS_UnBind(&stSrcChn, &stDestChn));

    return TS_SUCCESS;
}