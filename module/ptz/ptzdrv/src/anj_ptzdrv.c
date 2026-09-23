#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_ptzdrv.h"
#include "anj_ptz_provider.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <stdlib.h>

#define PTZ_DEV_NAME "anjptz"

#define SPEED_MULTIPLE (100) /*以ms为单位放大100倍*/

static pthread_mutex_t s_iPtzDriverMutex = PTHREAD_MUTEX_INITIALIZER;

static int anj_ptzdrv_control(int cmd, int arg)
{
    int iRet = PTZ_ERROR_OTHER;
    char cPtzDev[128];
    memset(cPtzDev, 0, sizeof(cPtzDev));
    snprintf(cPtzDev, sizeof(cPtzDev), "/dev/%s", PTZ_DEV_NAME);
    anj_mutex_lock(&s_iPtzDriverMutex);
    int fd = open(cPtzDev, O_RDWR);
    if (fd <= 0)
    {
        __ERR("open %s fail.\n", cPtzDev);
    }
    else
    {
        iRet = ioctl(fd, cmd, arg);
        close(fd);
    }
    anj_mutex_unlock(&s_iPtzDriverMutex);
    return iRet;
}

static int anj_ptzdrv_wait_timeset(int arg)
{
    return 0;
    int iRet = PTZ_ERROR_OTHER;
    int bOverTime = 0;
    while (1)
    {
        bOverTime++;
        iRet = anj_ptzdrv_control(PTZ_CTL_SPEED_SET, arg);
        if (iRet == PTZ_ERROR_TIMER_CTL_INVALID)
        {
            if (bOverTime >= PTZ_STOP_OVER_TIME)
            {
                __ERR("######PTZ Over Time!! %d\n", bOverTime);
                break;
            }
            usleep(PTZ_WAIT_TIME);
        }
        else
        {
            break;
        }
    }

    return iRet;
}

int anj_ptzdrv_operate(int mode, int arg, int speed)
{
    anj_ptzdrv_wait_timeset(speed * SPEED_MULTIPLE);
    int iRet = anj_ptzdrv_control(mode, arg);
    return iRet;
}

void anj_ptzdrv_dir_set(PtzDir *pstPtzDir)
{
    anj_ptzdrv_control(PTZ_CTL_HDIR_SET, pstPtzDir->HDir);
    anj_ptzdrv_control(PTZ_CTL_VDIR_SET, pstPtzDir->VDir);
}

void anj_ptzdrv_speed_set(PtzSpeed *pstPtzSpeed)
{
    return;
}

void anj_ptzdrv_debug()
{
    anj_ptzdrv_control(PTZ_CTL_DEBUG, 0);
}

int anj_ptzdrv_init()
{
    anj_mw_system("insmod /opt/ch/anj_ptz.ko");
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();
    anj_ptzdrv_dir_set(&pstIotPtzConfig->m_ptzDir);
    anj_ptzdrv_speed_set(&pstIotPtzConfig->m_ptzSpeed);

    return 0;
}

int anj_ptzdrv_uninit()
{
    anj_mw_system("rmmod /opt/ch/anj_ptz.ko");
    return 0;
}

static const anj_ptz_provider_ops s_stPtzDrvProviderOps = {
    .provider_name = "ptzdrv",
    .provider_priority = 200,
    .init = anj_ptzdrv_init,
    .uninit = anj_ptzdrv_uninit,
    .operate = anj_ptzdrv_operate,
    .debug = anj_ptzdrv_debug,
    .dir_set = anj_ptzdrv_dir_set,
    .speed_set = anj_ptzdrv_speed_set,
};

ANJ_LINK_KEEP(anj_keep_ptzdrv_provider);

__attribute__((constructor)) static void anj_ptzdrv_provider_register(void)
{
    anj_ptz_provider_register(&s_stPtzDrvProviderOps);
}

__attribute__((destructor)) static void anj_ptzdrv_provider_unregister(void)
{
    anj_ptz_provider_unregister(&s_stPtzDrvProviderOps);
}
