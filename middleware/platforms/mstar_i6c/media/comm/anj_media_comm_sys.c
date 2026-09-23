#include "anj_mw_media_common.h"
#include "anj_mw_time.h"

MI_S32 ST_Common_SysInit(void)
{
    // 降功耗
    //     anj_mw_system("echo 800000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq");
    //     anj_mw_system("echo 800000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq");
    //     anj_mw_system("echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor");
    //     anj_mw_system("echo 400 > /sys/dla/freq");
    //     anj_mw_system("echo 216000000 > /sys/class/mstar/isp0/isp_clk");
    //     anj_mw_system("echo 216000000 > /sys/devices/virtual/mstar/venc/ven_clock");
    //     anj_mw_system("echo 216000000 > /sys/devices/virtual/mstar/venc/ven_clock_2nd");
    //     anj_mw_system("echo 216000000 > /sys/devices/virtual/mstar/venc/ven_clock_axi");
    //     anj_mw_system("echo 216000000 > /sys/devices/virtual/mstar/venc/ven_clock_scdn");

    //     anj_mw_system("echo 172000000 > /sys/class/mstar/jpe/jpe_clock");

    //     anj_mw_system("echo 30 > /sys/class/gpio/export");
    //     anj_mw_system("echo out > /sys/class/gpio/gpio30/direction");
    //     anj_mw_system("echo 0 > /sys/class/gpio/gpio30/value");

    // #if defined(SUPPORT_AI_ISP)
    //     anj_mw_system("echo 1000000 > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq");
    //     anj_mw_system("echo 900 > /sys/dla/freq");
    // #endif

    MI_SYS_Version_t stVersion;
    MI_U64 u64Pts = 0;

    STCHECKRESULT(MI_SYS_Init(0));

    memset(&stVersion, 0x0, sizeof(MI_SYS_Version_t));
    STCHECKRESULT(MI_SYS_GetVersion(0, &stVersion));
    __INFO("u8Version:%s\n", stVersion.u8Version);

    STCHECKRESULT(MI_SYS_GetCurPts(0, &u64Pts));
    __INFO("u64Pts:%llu, %llu\n", u64Pts, anj_mw_get_cputime_ms(NULL) * 1000);

    u64Pts = anj_mw_get_cputime_ms(NULL) * 1000;
    STCHECKRESULT(MI_SYS_InitPtsBase(0, u64Pts));

    // u64Pts = 0xE1237890E1237890;
    STCHECKRESULT(MI_SYS_SyncPts(0, u64Pts));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SysUnInit(void)
{
    STCHECKRESULT(MI_SYS_Exit(0));

    return MI_SUCCESS;
}

MI_S32 ST_Common_SysBind(ST_Sys_BindInfo_T *pstBindInfo)
{
    __INFO("src(%d-%d-%d-%d)  dst(%d-%d-%d-%d)  %d...\n", pstBindInfo->stSrcChnPort.eModId, pstBindInfo->stSrcChnPort.u32DevId,
           pstBindInfo->stSrcChnPort.u32ChnId, pstBindInfo->stSrcChnPort.u32PortId,
           pstBindInfo->stDstChnPort.eModId, pstBindInfo->stDstChnPort.u32DevId, pstBindInfo->stDstChnPort.u32ChnId,
           pstBindInfo->stDstChnPort.u32PortId, pstBindInfo->eBindType);

    ExecFunc(MI_SYS_BindChnPort2(0, &pstBindInfo->stSrcChnPort, &pstBindInfo->stDstChnPort,
                                 pstBindInfo->u32SrcFrmrate, pstBindInfo->u32DstFrmrate, pstBindInfo->eBindType, pstBindInfo->u32BindParam),
             MI_SUCCESS);
    return MI_SUCCESS;
}

MI_S32 ST_Common_SysUnBind(ST_Sys_BindInfo_T *pstBindInfo)
{
    ExecFunc(MI_SYS_UnBindChnPort(0, &pstBindInfo->stSrcChnPort, &pstBindInfo->stDstChnPort), MI_SUCCESS);

    return MI_SUCCESS;
}

int ST_Common_DumpFile(MI_SYS_ChnPort_t *pstChnPort, char *FileName)
{
    int dump_num = 0;
    MI_S32 s32Fd = 0;
    fd_set read_fds;
    struct timeval TimeoutVal;

    STCHECKRESULT(MI_SYS_SetChnOutputPortDepth(0, pstChnPort, 2, 4));

    STCHECKRESULT(MI_SYS_GetFd(pstChnPort, &s32Fd));

    while (dump_num < 3)
    {

        FD_ZERO(&read_fds);
        FD_SET(s32Fd, &read_fds);
        TimeoutVal.tv_sec = 1;
        TimeoutVal.tv_usec = 0;
        MI_S32 s32Ret = select(s32Fd + 1, &read_fds, NULL, NULL, &TimeoutVal);

        if (s32Ret < 0)
        {
            __ERR("select fail\n");
            usleep(1000 * 1000);
            continue;
        }
        else if (s32Ret == 0)
        {
            __ERR("select timeout\n");
            usleep(1000 * 1000);
            continue;
        }
        else
        {
            if (FD_ISSET(s32Fd, &read_fds))
            {
                FD_CLR(s32Fd, &read_fds);

                MI_SYS_BufInfo_t stBufInfo;
                memset(&stBufInfo, 0, sizeof(MI_SYS_BufInfo_t));

                MI_SYS_BUF_HANDLE hHandle;

                s32Ret = MI_SYS_ChnOutputPortGetBuf(pstChnPort, &stBufInfo, &hHandle);

                if (s32Ret != MI_SUCCESS)
                {
                    __ERR("MI_SYS_ChnOutputPortGetBuf faild!\n");
                    break;
                }

                char FilePath[192] = {'0'};

                sprintf(FilePath, "./%d%s", dump_num, FileName);

                FILE *fp = NULL;
                fp = fopen(FilePath, "wb+");
                if (fp == NULL)
                {
                    __ERR("fopen %s fail!!!\n", FilePath);

                    goto OutputPortPutBuf;
                }

                int write_count = 0;

                write_count = fwrite((MI_U8 *)stBufInfo.stFrameData.pVirAddr[0], 1, stBufInfo.stFrameData.u32BufSize, fp);

                if (write_count <= 0)
                {
                    __ERR("write file err \n");
                }
                else
                {
                    __INFO("\n########## FilePath=%s\n\n", FilePath);
                    dump_num++;
                }

            OutputPortPutBuf:
                if (MI_SUCCESS != MI_SYS_ChnOutputPortPutBuf(hHandle))
                {
                    __ERR("(%d-%d-%d-%d) put buf error \n", pstChnPort->eModId, pstChnPort->u32DevId,
                          pstChnPort->u32ChnId, pstChnPort->u32PortId);
                }

                if (fp)
                {
                    fclose(fp);
                    fp = NULL;
                }
            }
        }
    }

    STCHECKRESULT(MI_SYS_CloseFd(s32Fd));

    return MI_SUCCESS;
}

char *ST_Common_SysMmap(unsigned long long u64PhyAddr, unsigned int mapsize)
{
    void *pMappedAddr = NULL;
    MI_SYS_Init(0);
    if (MI_SYS_Mmap(u64PhyAddr, mapsize, &pMappedAddr, 1) != MI_SUCCESS)
    {
        return pMappedAddr;
    }
    if (MI_SYS_FlushInvCache(pMappedAddr, mapsize) != 0)
    {
        __ERR("phys_addr = %#x, MI_SYS_FlushInvCache failed\n", u64PhyAddr);

        ST_Common_SysMunmap(pMappedAddr, mapsize);
        pMappedAddr = NULL;
    }

    return pMappedAddr;
}

void ST_Common_SysMunmap(void *pVirtualAddress, unsigned int mapsize)
{
    MI_SYS_Munmap(pVirtualAddress, mapsize);
}

MI_S32 ST_Common_SysMma_Alloc(unsigned char *pstMMAHeapName, unsigned int u32BlkSize ,unsigned long long *phyAddr)
{
    MI_SYS_Init(0);

    STCHECKRESULT(MI_SYS_MMA_Alloc(0, pstMMAHeapName, u32BlkSize, phyAddr));

    return MI_SUCCESS;
}

void ST_Common_SysMma_Free(unsigned long long phyAddr)
{
    MI_SYS_MMA_Free(0, phyAddr);
}