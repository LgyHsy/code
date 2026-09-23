#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <errno.h>

#include "anj_config.h"
#include "eventhub.h"
#include "anj_mw_comm.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_thread.h"
#include "anj_sysmng.h"
#include "function_list.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_video.h"
#include "anj_mw_time.h"

#include "anj_mcu.h"
#include "anj_mw_aov.h"

#define SYS_POWER_STATE                 "/sys/power/state"
#define SYS_PM_FREEZE_TIMEOUT           "/sys/power/pm_freeze_timeout"
#define SYS_SUSPEND_FAILED_FREEZE       "/sys/power/suspend_stats/failed_freeze"
#define PM_FREEZE_EXPECT_TIME           "500"

#define AOV_POWER_DOWN_DELAY_TIME   10      // 10ms
#define AOV_HEARTBEAT_INTERVAL      300     // 300s
#define AOV_HUMAN_CHECK_INTERVAL    6000    // 6000ms

#define AOV_PRINT_DIFF_TIME         0

typedef struct
{
    unsigned int ucFps;
    unsigned short usSleepTimeMs;
}AovFpsSleepTime;

static AnjAovCtrlInfo_t sAnjAovCtrlInfo;
static anj_thread_s s_AovCtrlThread;

static pthread_mutex_t sAovCtrlMutex = PTHREAD_MUTEX_INITIALIZER;

//上电有一个60-80ms的差值，按最大补
AovFpsSleepTime g_AovFpsToSleepTimeMap[] = {
    {1,  920},
    {2,  1920},
    {3,  2920},
    {4,  3920},
    {5,  4920}
};

static unsigned short aov_get_sleep_time_ms(unsigned int ucAovFps)
{
    int i =0;
    unsigned short usTimeMs = g_AovFpsToSleepTimeMap[0].usSleepTimeMs; //默认是1s1帧
    int cnt = sizeof(g_AovFpsToSleepTimeMap) / sizeof(g_AovFpsToSleepTimeMap[0]);
    for(i = 0; i < cnt; i++)
    {
        if(ucAovFps ==  g_AovFpsToSleepTimeMap[i].ucFps)
        {
            usTimeMs = g_AovFpsToSleepTimeMap[i].usSleepTimeMs;
        }
    }

    return usTimeMs;
}


static long long calculate_diff_time_ms(struct timeval *pstBeforeStamp, struct timeval *pstAfterStamp)
{
    long long diff_sec = pstAfterStamp->tv_sec - pstBeforeStamp->tv_sec;
    long long diff_usec = pstAfterStamp->tv_usec - pstBeforeStamp->tv_usec;
    
    return (diff_sec * 1000000 + diff_usec) / 1000;
}


// just for debug: check freeze timeout
static int anj_aov_freeze_timeout_check(void)
{
    int iRet = 0;
    int fd = 0;
    static int iFirstSetFlag = 1;
    static char au8CurrentFailedFreezeCnt[128] = {0};
    static char au8LastFailedFreezeCnt[128] = {0};
    
    if (1 == iFirstSetFlag)
    {
        iFirstSetFlag = 0;

        iRet = write_sys_file_str(SYS_PM_FREEZE_TIMEOUT, PM_FREEZE_EXPECT_TIME);

        fd = open(SYS_SUSPEND_FAILED_FREEZE, O_RDONLY);
        if (fd < 0)
        {
            __ERR("open %s failed!\n", SYS_SUSPEND_FAILED_FREEZE);
            return -1;
        }
        else
        {
            lseek(fd, 0, SEEK_SET);
            read(fd, au8CurrentFailedFreezeCnt, sizeof(au8CurrentFailedFreezeCnt));

            lseek(fd, 0, SEEK_SET);
            close(fd);

            memcpy(au8LastFailedFreezeCnt, au8CurrentFailedFreezeCnt, sizeof(au8LastFailedFreezeCnt));
        }
    }
    else
    {
        fd = open(SYS_SUSPEND_FAILED_FREEZE, O_RDONLY);
        if (fd < 0)
        {
            __ERR("open %s failed!\n", SYS_SUSPEND_FAILED_FREEZE);
            return -1;
        }
        else
        {
            lseek(fd, 0, SEEK_SET);
            read(fd, au8CurrentFailedFreezeCnt, sizeof(au8CurrentFailedFreezeCnt));

            lseek(fd, 0, SEEK_SET);
            close(fd);

            iRet = strncmp(au8LastFailedFreezeCnt, au8CurrentFailedFreezeCnt, sizeof(au8CurrentFailedFreezeCnt));
            if (iRet)
            {
                __ERR("Failed_freeze increased\n");
                memcpy(au8LastFailedFreezeCnt, au8CurrentFailedFreezeCnt, sizeof(au8LastFailedFreezeCnt));
            }
        }
    }

    return iRet;
}



static int anj_aov_4g_resume()
{
    anj_mw_hwctrl_aov_4g_resume_status(1);
    usleep(10 * 1000);
    anj_mw_hwctrl_aov_4g_resume_status(0);    
}

int anj_aov_set_wakeup_battery(int battery)
{
    pthread_mutex_lock(&sAovCtrlMutex);
    if (battery != sAnjAovCtrlInfo->iBatteryCapacity)
    {
        __INFO("Device wakeup event:%d change to %d\n", sAnjAovCtrlInfo->iBatteryCapacity, battery);
        sAnjAovCtrlInfo->iBatteryCapacity = battery;
    }
    pthread_mutex_unlock(&sAovCtrlMutex);

    return 0;
}

static void anj_aov_trigger_human(EventResult *event_result, void *data)
{
    if (event_result && data)
    {
        pthread_mutex_lock(&sAovCtrlMutex);
        unsigned long long uTime = *(unsigned long long *)data;
        sAnjAovCtrlInfo->uHumanDetectTime = uTime;
        sAnjAovCtrlInfo->eCurDetResult = E_DetResult_Detected;
        pthread_mutex_unlock(&sAovCtrlMutex);

        event_result->ret = 0;
    }
}

static void anj_aov_notify_encode(EventResult *event_result, void *data)
{
    if (event_result && data)
    {
        aov_notify_enc_t notify_event = *(aov_notify_enc_t *)data;
        if (AOV_NOTIFY_ENC_START == notify_event->event)
        {
            event_result->ret = anj_mw_aov_notify_enc_start();
        }
        else if (AOV_NOTIFY_ENC_DONE == notify_event->event)
        {
            event_result->ret = anj_mw_aov_notify_enc_done();
        }
        else if (AOV_NOTIFY_ENC_CHANGE == notify_event->event)
        {
            event_result->ret = anj_mw_aov_notify_enc_status_chg(notify_event->status);
        }
        else
        {
            __ERR("aov notify encode type:%d, status:%d! error!\n", notify_event->event);
            event_result->ret = -1;
        }
    }
}

static void anj_aov_notify_algo_detect(EventResult *event_result, void *data)
{
    if (event_result && data)
    {
        aov_notify_algo_t notify_event = *(aov_notify_algo_t *)data;
        if (AOV_NOTIFY_ALGO_START == notify_event->event)
        {
            event_result->ret = anj_mw_aov_notify_algo_start();
        }
        else if (AOV_NOTIFY_ALGO_DONE == notify_event->event)
        {
            event_result->ret = anj_aov_notify_algo_done();
        }
        else if (AOV_NOTIFY_ALGO_CHANGE == notify_event->event)
        {
            event_result->ret = anj_aov_notify_algo_change(notify_event->status);
        }
        else
        {
            __ERR("aov notify algo detect type:%d error!\n", notify_event->event);
            event_result->ret = -1;
        }
    }
}


int anj_aov_attr_init(AnjAovCtrlInfo_t *pAnjAovCtrlInfo)
{
    MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediaCfg->videoConfig[0].videoCapture;

    int iWorkMode = pVideoCapCfg->aov_mode;
    pAnjAovCtrlInfo->iUserWorkMode = anj_mw_check_value_in_range(iWorkMode, E_USR_AOV_MODE_NORMAL, E_USR_AOV_MODE_AOV);

    int iAovFps = pVideoCapCfg->aov_fps;
    pAnjAovCtrlInfo->iUserAovFps = anj_mw_check_value_in_range(iAovFps, E_USR_AOV_PER_FRAME_SEC_1, E_USR_AOV_PER_FRAME_SEC_4);

    pAnjAovCtrlInfo->eCurRunStat = E_RUN_STAT_NORMAL;

    pAnjAovCtrlInfo->eCurFpsType = E_FpsType_High;
    pAnjAovCtrlInfo->eLastFpsType = E_FpsType_High;

    pAnjAovCtrlInfo->eCurWakeUpEvent = E_WAKEUP_EVENT_POWER_ON;

    pAnjAovCtrlInfo->iChargeStatus = CHARGING_OFF;
    pAnjAovCtrlInfo->iBatteryCapacity = -1;

    pAnjAovCtrlInfo->eCurDetResult = E_DetResult_UnDetected;
}

int anj_aov_check_algo_result(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    int iCheckHdTime = 0;
    unsigned long long nowtime = anj_mw_get_cputime_ms(NULL);

    pthread_mutex_lock(&sAovCtrlMutex);
    if (nowtime - pAovCtrlInfo->uHumanDetectTime >= AOV_HUMAN_CHECK_INTERVAL)
    {
        iCheckHdTime = 0;
    }
    else
    {
        iCheckHdTime = 1;
    }

    int iAovMode = pAovCtrlInfo->iUserWorkMode;

    E_DetResult CurDetResult = pAovCtrlInfo->eCurDetResult;
    E_DetResult LastDetResult = CurDetResult;
    if (E_DetResult_Detected == CurDetResult && 1 == iCheckHdTime)
    {
        CurDetResult = E_DetResult_Detected;
    }
    else
    {
        CurDetResult = E_DetResult_UnDetected;
    }

    if (CurDetResult != LastDetResult)
    {
        __INFO("Algo detect last result:%d change to now:%d\n", LastDetResult, CurDetResult);

        if (E_DetResult_Detected == CurDetResult)
        {
            if (0 == access("/tmp/aov.flag", F_OK))
            {
                __INFO("aov resume 4g module\n");
                anj_aov_4g_resume();
            }
            else if (E_USR_AOV_MODE_LOWBAT == iAovMode)
            {
                __INFO("aov resume 4g module\n");
                anj_aov_4g_resume();
            }
        }

        pAovCtrlInfo->eCurDetResult = CurDetResult;
    }
    pthread_mutex_unlock(&sAovCtrlMutex);

    return 0;
}

static int anj_aov_check_4g_stat(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    static int sLast4gStatValue = -1;
    E_McuWakeUpSrc eMcuWakeupSrc = E_MCU_WAKEUP_SRC_ALWAYS_ON;

    int iAovMode = pAovCtrlInfo->iUserWorkMode;
    if (E_USR_AOV_MODE_AOV == iAovMode)
    {
        int iNow4gStatValue = -1;
        iNow4gStatValue = anj_mw_hwctrl_aov_4g_stat_get();

        if(iNow4gStatValue != sLast4gStatValue)
        {
            __INFO("aov 4g stat gpio last value:%d change to now:%d\n", sLast4gStatValue, iNow4gStatValue);
            sLast4gStatValue = iNow4gStatValue;
        }

        if (1 == iNow4gStatValue)
        {
            eMcuWakeupSrc = E_MCU_WAKEUP_SRC_NET;
        }
        else
        {
            eMcuWakeupSrc = E_MCU_WAKEUP_SRC_TIMER;
        }
    }

    pAovCtrlInfo->eWakeupSrcToMcu = eMcuWakeupSrc;
    return 0;
}

static int anj_aov_check_motor_status()
{
    if(0 != access("/tmp/lowpower.flag", F_OK))
    {
        if(access("/tmp/flag.ptzreset.finish", F_OK) != 0 || access("/tmp/in_guard_pos", F_OK) != 0)
        {
            return 1;
        }
    }

    return 0;
}

static int anj_aov_check_service_stat(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    /*以下条件不进入AOV模式
    1. 产测模式 
    2. 有线模式
    3. 电机在转动
    4. 没检测到P2P第一次上线
    6. P2P未绑定
    7. 检测到复位动作
    8. 正常电量没在预置位或没检测到电机自检完成，低电量不检测这两项
    9. 检测到sd里边存在调试标识，不休眠
    */

    // 这里后续改成获取状态的函数
    // todo...

    int iRet = 0;
    int net_wire_status = 0;
    DevInfo *pstDevInfo = getDevInfo();
    {
        anj_net_status_e status =  anj_net_status_check();
        if (ANJ_NET_STATUS_WIRE == status)
        {
            net_wire_status = 1;
        }
    }
    
    if((pstDevInfo != NULL && pstDevInfo->bFactoryMode)
        || 1 == net_wire_status 
        //|| 1 == g_ptz_turnstatus
        || access("/tmp/p2p_online.flag", F_OK) != 0
        || access("/mnt/nand/device.bind.flag", F_OK) != 0 
        || access("/tmp/key_press.flag", F_OK) == 0
        || access("/tmp/reset_button_press.flag", F_OK) == 0
        || access("/tmp/g4_drivers_remove.flag", F_OK) != 0
        || 1 == anj_aov_check_motor_status()
        || access("/mnt/mmc0/debug.flag", F_OK) == 0)
    {
        pAovCtrlInfo->eWakeupSrcToMcu = E_MCU_WAKEUP_SRC_ALWAYS_ON;
    }

    //测试指令，判断有此文件，则设置模式为长电模式，优先测试文件，后续可屏蔽
    if(0 == access("/tmp/timer", F_OK))
    {
        pAovCtrlInfo->eLastWakeupSrcToMcu = E_MCU_WAKEUP_SRC_TIMER;
    }

    if (pAovCtrlInfo->eWakeupSrcToMcu != pAovCtrlInfo->eLastWakeupSrcToMcu)
    {
        __INFO("aov wakeup last stat:%d change to now:%d! ret:%d\n", pAovCtrlInfo->eLastWakeupSrcToMcu, pAovCtrlInfo->eWakeupSrcToMcu, iRet);
        iRet = anj_mcu_set_wakeup_src((unsigned int) pAovCtrlInfo->eWakeupSrcToMcu);;
    }

    return 0;
}

static int anj_aov_check_net_ko_stat(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    int iRet = 0;
    if (pAovCtrlInfo->eWakeupSrcToMcu != pAovCtrlInfo->eLastWakeupSrcToMcu)
    {
        if (E_MCU_WAKEUP_SRC_TIMER == pAovCtrlInfo->eWakeupSrcToMcu)
        {
            iRet = SearchStringInCmd("lsmod", "sstar_emac");
            if (1 == iRet)
            {
                __INFO("aov rmmod sstar_emac!\n");
                anj_mw_system("rmmod sstar_emac");
            }

            //关闭内核打印
            __INFO("aov close kernel print!\n");
            anj_mw_system("echo 0 > /proc/sys/kernel/printk");
        }
        else
        {
            //恢复内核打印            
            __INFO("aov recover kernel print\n");
            anj_mw_system("echo 4 > /proc/sys/kernel/printk");
        }
    }

    return 0;
}

static int anj_aov_low_fps_set(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    int index = 0;
    pAovCtrlInfo->eLastFpsType = pAovCtrlInfo->eCurFpsType;
    pAovCtrlInfo->eCurFpsType = E_FpsType_Low;

    if (E_FpsType_High == pAovCtrlInfo->eLastFpsType)
    {
        MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
        VideoEncode *pVencCfg = &mediacfg->videoConfig[0].videoEncode;

        // 修改编码参数
        for(int iIndex = 0; iIndex < 2; iIndex ++)
        {
            //int nGop = pVencCfg.encodeCfg[index].initQuant;
            int nFrame = pVencCfg.encodeCfg[index].frameRate;
            //int nBitRate = pVencCfg.encodeCfg[index].bitRate;

            anj_video_set_gop(iIndex, 8);
            anj_video_set_fps(iIndex, nFrame);
        }

        // 停掉 SCL Chn
        anj_video_scl_set(0);

        // 关闭音频
        module_uninit_single("anj_audio");

        // 创建标志文件
        if (access("/tmp/aov.flag", F_OK) != 0)
        {
            __INFO("touch /tmp/aov.flag\n");
            anj_mw_system("touch /tmp/aov.flag");
        }
    }

    // 进入睡眠模式
    anj_mw_aov_sys_sleep_enter();

    // 请求I帧
    if (pAovCtrlInfo->eLastFpsType != pAovCtrlInfo->eCurFpsType)
    {
        anj_video_request_idr(0, 0);
        anj_video_request_idr(0, 1);
    }

    // 重新开启 SCL Chn
    if (E_FpsType_High == pAovCtrlInfo->eLastFpsType)
    {
        anj_video_scl_set(1);
    }

    pAovCtrlInfo->eCurRunStat = E_RUN_STAT_AOV;
}

static int anj_aov_high_fps_set(AnjAovCtrlInfo_t *pAovCtrlInfo)
{
    int index = 0;
    EventResult event_result = {0};
    int enable = 0;

    pAovCtrlInfo->eLastFpsType = pAovCtrlInfo->eCurFpsType;
    pAovCtrlInfo->eCurFpsType = E_FpsType_High;

    if (E_FpsType_Low == pAovCtrlInfo->eLastFpsType)
    {
        MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
        VideoEncode *pVencCfg = &mediacfg->videoConfig[0].videoEncode;

        // 修改编码参数
        for (index = 0; index < 2; index++)
        {
            int nGop = pVencCfg.encodeCfg[index].initQuant;
            int nFrame = pVencCfg.encodeCfg[index].frameRate;
            int nBitRate = pVencCfg.encodeCfg[index].bitRate;

            anj_video_set_gop(index, nGop);
            anj_video_set_fps(index, nFrame);
            anj_video_set_bitrate(index, nBitRate);

        }

        // 退出睡眠模式
        anj_mw_aov_sys_sleep_exit();

        // 打开音频
        module_init_single("anj_audio");
        // 删除文件
        if(0 == access("/tmp/aov.flag", F_OK))
        {
            __INFO("rm /tmp/aov.flag\n");
            anj_mw_system("rm /tmp/aov.flag");
        }
    }

    // 请求I帧
    if (pAovCtrlInfo->eLastFpsType != pAovCtrlInfo->eCurFpsType)
    {
        anj_video_request_idr(0, 0);
        anj_video_request_idr(0, 1);
    }

    pAovCtrlInfo->eCurRunStat = E_RUN_STAT_NORMAL;
}


static int anj_aov_enter_suspend()
{
    int iRet = 0;

#if AOV_PRINT_DIFF_TIME
    static struct timeval st = {0};
    static struct timeval ed = {0};
    long long diff = 0;
    static struct timeval stCurTime1 = {0};
    static struct timeval stCurTime2 = {0};
    long long s32DiffTime = 0;
    gettimeofday(&st, NULL);
#endif

    E_McuWakeUpSrc eMcuWakeupSrc = E_MCU_WAKEUP_SRC_TIMER;
    iRet = anj_mcu_set_wakeup_src(unsigned int (eMcuWakeupSrc));
    if (iRet)
    {
        __ERR("aov set mcu wake up src failed!\n");
        anj_mw_aov_sys_sleep_exit();
        anj_mw_aov_sys_sleep_enter();
        return iRet;
    }

#if AOV_PRINT_DIFF_TIME
    gettimeofday(&stCurTime2, NULL);
    diff = calculate_diff_time_ms(&st, &stCurTime2);
    __INFO("diff time:%lld\n", diff);
    
    __INFO("current time2:%ld s,%ld us\n", stCurTime2.tv_sec, stCurTime2.tv_usec);
    s32DiffTime = (((stCurTime2.tv_sec - stCurTime1.tv_sec) * 1000 * 1000) + (stCurTime2.tv_usec - stCurTime1.tv_usec)) / 1000;
    __INFO("s32DiffTime: %lld ms\n", s32DiffTime);
#endif

    iRet = write_sys_file_str(SYS_POWER_STATE, "mem");
    if (iRet)
    {
        return iRet;
    }

#if AOV_PRINT_DIFF_TIME
    printf("\033[34m=>\n\033[0m");
    gettimeofday(&stCurTime1, NULL);
    printf("current time1:%ld s, %ld us\n", stCurTime1.tv_sec, stCurTime1.tv_usec);
#endif

    anj_aov_freeze_timeout_check();

#if AOV_PRINT_DIFF_TIME
    gettimeofday(&ed, NULL);
    diff = calculate_diff_time_ms(&stCurTime1, &ed);
    __INFO("diff time:%lld\n", diff);
#endif

    return 0;
}

static int anj_aovctrl_thread(void *ctx, int *bStart)
{
    AnjAovCtrlInfo_t *pAovCtrlInfo = (AnjAovCtrlInfo_t *)ctx;

    // step1. mcu先于aov初始化，等待mcu升级
    int mcu_upgrade_flag = 0;
    while(bStart && 1 == *bStart)
    {
        mcu_upgrade_flag = anj_mcu_get_upgrade_stat();
        if (0 == mcu_upgrade_flag)
        {
            break;
        }

        usleep(500 * 1000);
    }

    // step2. 设置mcu参数
    int mcu_init_flag = anj_mcu_get_init_stat();
    if (0 == mcu_init_flag)
    {
        __ERR("mcu init failed!\n");
        goto exit_aov;
    }

    anj_mcu_set_power_down_delay_ms(AOV_POWER_DOWN_DELAY_TIME);
    anj_mcu_lamp_ctrl(E_LampType_WhiteLed, 0);
    usleep(10 * 1000);
    anj_mcu_set_wakeup_src((unsigned int)E_WakeupSrc_AlwaysOn);
    anj_mcu_set_heartbeat_interval(AOV_HEARTBEAT_INTERVAL);

    unsigned short usSupspendMs = 0;
    usSupspendMs = aov_get_sleep_time_ms(pAovCtrlInfo->iUserAovFps);
    anj_mcu_set_wakeup_timer_interval(usSupspendMs);

    struct timeval stcurrent = {0};
    struct timeval stbegin = {0};
    struct timeval stResumeTimeDiff = {0};
    long long sleep_diff_ms = 0;

#if AOV_PRINT_DIFF_TIME
    struct timeval st = {0};
    struct timeval ed = {0};
    long long diff_ms = 0;
#endif

    gettimeofday(&(pAovCtrlInfo->stResumeTime), NULL);

    while(bStart && 1 == *bStart)
    {
#if AOV_PRINT_DIFF_TIME
        gettimeofday(&st, NULL);
#endif
        // 等待编码完成
        anj_aov_notify_wait_encode_done();

#if AOV_PRINT_DIFF_TIME
        gettimeofday(&ed, NULL);
        diff_ms = calculate_diff_time_ms(&st, &ed);
        __WARN("diff time:%lldms\n", diff_ms);
#endif

        // 等待算法识别完成
        anj_aov_notify_wait_algo_done();

#if AOV_PRINT_DIFF_TIME
        gettimeofday(&st, NULL);
        diff_ms = calculate_diff_time_ms(&ed, &st);
        __WARN("diff time:%lldms\n", diff_ms);
#endif

        // 检测算法、4g stat引脚状态、外部业务、驱动状态
        anj_aov_check_algo_result(pAovCtrlInfo);

        anj_aov_check_4g_stat(pAovCtrlInfo);

        anj_aov_check_service_stat(pAovCtrlInfo);

        anj_aov_check_net_ko_stat(pAovCtrlInfo);

#if AOV_PRINT_DIFF_TIME
        gettimeofday(&ed, NULL);
        diff_ms = calculate_diff_time_ms(&st, &ed);
        __WARN("diff time:%lldms\n", diff_ms);
#endif

        E_FpsType eNowFpsType = E_FpsType_High;
        switch (pAovCtrlInfo->eWakeupSrcToMcu)
        {
            case E_MCU_WAKEUP_SRC_TIMER:
            {
                if (E_DetResult_UnDetected == pAovCtrlInfo->eCurDetResult)
                {
                    anj_aov_low_fps_set(pAovCtrlInfo);

#if AOV_PRINT_DIFF_TIME
                    gettimeofday(&st, NULL);
                    diff_ms = calculate_diff_time_ms(&ed, &st);
                    __WARN("diff time:%lldms\n", diff_ms);
#endif

                    gettimeofday(&stcurrent, NULL);
                    sleep_diff_ms = calculate_diff_time_ms(&stbegin, &stcurrent);

                    do 
                    {
                        gettimeofday(&stResumeTimeDiff, NULL);
                        sleep_diff_ms = calculate_diff_time_ms(&(pAovCtrlInfo->stResumeTime), &stResumeTimeDiff);

                        if (sleep_diff_ms < 1000)
                        {
                            // 这里要加上写录像状态的判断
                            // todo...

                            int nIsWritingRecord = 1;
                            if (1 == nIsWritingRecord)
                            {
                                usleep(1 * 1000);
                            }
                            else
                            {
                                anj_aov_enter_suspend();
                                break;
                            }
                        }
                        else
                        {
                            //超时还没处理完成
                            __ERR("Continue to handle the next frame while something timeout close sleepMode\n");

                            // 先关闭休眠模式
                            anj_mw_aov_sys_sleep_exit();

                            // 重新配置休眠模式
                            anj_mw_aov_sys_sleep_enter();
                        }
                    } while(1);
                }
                else
                {
                    anj_aov_high_fps_set(pAovCtrlInfo);
                }
            
                break;
            }

            case E_MCU_WAKEUP_SRC_NET:
            case E_MCU_WAKEUP_SRC_ALWAYS_ON:
            {
                anj_aov_high_fps_set(pAovCtrlInfo);

                break;
            }

            default:
            {
                __ERR("aov error mcu wakeup src:%d\n", pAovCtrlInfo->eWakeupSrcToMcu);
                goto exit_aov;
            }
            
        }

        pAovCtrlInfo->eLastWakeupSrcToMcu = pAovCtrlInfo->eWakeupSrcToMcu;
        gettimeofday(&stbegin, NULL);
        gettimeofday(&(pAovCtrlInfo->stResumeTime), NULL);

        // 通知开始编码
        anj_aov_notify_encode_start();

        // 通知开始算法检测
        anj_aov_notify_algo_start();

    }

exit_aov:
    __ERR("aov thread exit!\n");
    usleep(500 * 1000);

    aov_sleep_exit();

    return 0;
}


int anj_aov_init()
{
    int iRet = 0;
    anj_aov_attr_init(&sAnjAovCtrlInfo);

    memset(&s_AovCtrlThread, 0, sizeof(anj_thread_s));
    s_AovCtrlThread.bAutoDestroy = 1;
    strncpy(s_AovCtrlThread.iThreadName, "anj_aovctrl_thread", sizeof(s_AovCtrlThread.iThreadName) - 1);
    s_AovCtrlThread.iThreadjob.ctx = &sAnjAovCtrlInfo;
    s_AovCtrlThread.iThreadjob.func = anj_aovctrl_thread;
    iRet = anj_thread_task_create(&s_AovCtrlThread);
    if (iRet)
    {
        __ERR("create anj_aovctrl_thread failed\n");
    }

    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_AOV_TRIGGER_HUMAN, anj_aov_trigger_human);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_AOV_NOTIFY_ENCODE, anj_aov_notify_encode);
    eventhub_subscribe(EVENTHUB_CLASS_CTRL, EVENTHUB_AOV_NOTIFY_ALGO, anj_aov_notify_algo_detect);

    return 0;
}

int anj_aov_uninit()
{
    anj_thread_task_destroy(&s_AovCtrlThread, 0);

    return 0;
}
