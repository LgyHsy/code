#include "anj_mw_comm.h"
#include "anj_config.h"
#include "anj_ptziic.h"
#include "anj_ptz_provider.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define PTZIIC_DEV "/dev/anjgc615"
#define PTZIIC_DEV_MAJOR (97)
#define PTZIIC_MIN_MOVE_STEP (5) /* 小于此步数电机实际不转，直接拒绝避免坐标虚增 */

static pthread_mutex_t s_stPtzIicMutex = PTHREAD_MUTEX_INITIALIZER;
static int s_iPtzIicFd = -1;

/* anj_ptziic.ko 由 rootfs 的 loadko 在 anjcam 之前加载，这里只惰性打开节点 */
static int anj_ptziic_dev_get(void)
{
    if (s_iPtzIicFd > 0)
    {
        return s_iPtzIicFd;
    }

    s_iPtzIicFd = open(PTZIIC_DEV, O_RDWR);
    if (s_iPtzIicFd <= 0)
    {
        mysystem_with_param("mknod %s c %d 0", PTZIIC_DEV, PTZIIC_DEV_MAJOR);
        s_iPtzIicFd = open(PTZIIC_DEV, O_RDWR);
    }

    return s_iPtzIicFd;
}

static int anj_ptziic_control(int cmd, int arg)
{
    int iRet = PTZ_ERROR_OTHER;

    anj_mutex_lock(&s_stPtzIicMutex);
    if (anj_ptziic_dev_get() > 0)
    {
        iRet = ioctl(s_iPtzIicFd, cmd, arg);
    }
    else
    {
        __ERR("open %s fail.\n", PTZIIC_DEV);
    }
    anj_mutex_unlock(&s_stPtzIicMutex);

    return iRet;
}

int anj_ptziic_operate(int mode, int arg, int speed)
{
    if (mode >= PTZ_CTL_MOTOR_UP && mode <= PTZ_CTL_MOTOR_RIGHT)
    {
        if (arg < PTZIIC_MIN_MOVE_STEP)
        {
            /* 不发 SPEED/MOTOR；返回 -1 让上层不更新坐标（return 0 会虚增） */
            return -1;
        }
        /* 速度要在电机起步前落到芯片周期寄存器，驱动内部按 PTZ_SPEED_* 映射 */
        anj_ptziic_control(PTZ_CTL_SPEED_SET, speed);
    }

    return anj_ptziic_control(mode, arg);
}

void anj_ptziic_dir_set(PtzDir *pstPtzDir)
{
    anj_ptziic_control(PTZ_CTL_HDIR_SET, pstPtzDir->HDir);
    anj_ptziic_control(PTZ_CTL_VDIR_SET, pstPtzDir->VDir);
}

void anj_ptziic_debug()
{
    anj_ptziic_control(PTZ_CTL_DEBUG, 0);
}

int anj_ptziic_init()
{
    IotPtzConfig *pstIotPtzConfig = getIotPtzConfig();

    if (anj_ptziic_dev_get() <= 0)
    {
        __ERR("%s open fail, check anj_ptziic.ko loaded\n", PTZIIC_DEV);
        return -1;
    }

    anj_ptziic_dir_set(&pstIotPtzConfig->m_ptzDir);

    return 0;
}

int anj_ptziic_uninit()
{
    anj_mutex_lock(&s_stPtzIicMutex);
    if (s_iPtzIicFd > 0)
    {
        close(s_iPtzIicFd);
        s_iPtzIicFd = -1;
    }
    anj_mutex_unlock(&s_stPtzIicMutex);

    return 0;
}

static const anj_ptz_provider_ops s_stPtzIicProviderOps = {
    .provider_name = "ptziic",
    .provider_priority = 100,
    .init = anj_ptziic_init,
    .uninit = anj_ptziic_uninit,
    .operate = anj_ptziic_operate,
    .debug = anj_ptziic_debug,
    .dir_set = anj_ptziic_dir_set,
    .speed_set = 0,
};

ANJ_LINK_KEEP(anj_keep_ptziic_provider);

__attribute__((constructor)) static void anj_ptziic_provider_register(void)
{
    anj_ptz_provider_register(&s_stPtzIicProviderOps);
}

__attribute__((destructor)) static void anj_ptziic_provider_unregister(void)
{
    anj_ptz_provider_unregister(&s_stPtzIicProviderOps);
}
