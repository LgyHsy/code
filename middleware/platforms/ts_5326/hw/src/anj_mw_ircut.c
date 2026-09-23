#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_ircut.h"

#define IRCUT_IRLED_DEV       "/dev/irled"
#define IRCUT_IRLED_MAJOR     (93)
#define IRCUT_GC615_DEV       "/dev/anjgc615"
#define IRCUT_GC615_MAJOR     (97)
#define IRCUT_GC615_IOCTL     (30) /* 与 prebuild/anj_ptziic 一致 */
#define IRCUT_GC615_MODULE    "/sys/module/anj_ptziic"

static int s_IrcutCtrlFd = -1;
static int s_bUseGc615 = 0;

static int anj_mw_ircut_use_gc615(void)
{
    return (access(IRCUT_GC615_MODULE, F_OK) == 0);
}

static int anj_mw_ircut_open()
{
    const char *pDev = NULL;
    int iMajor = 0;

    if (s_IrcutCtrlFd > 0)
    {
        return s_IrcutCtrlFd;
    }

    s_bUseGc615 = anj_mw_ircut_use_gc615();
    if (s_bUseGc615)
    {
        pDev = IRCUT_GC615_DEV;
        iMajor = IRCUT_GC615_MAJOR;
    }
    else
    {
        pDev = IRCUT_IRLED_DEV;
        iMajor = IRCUT_IRLED_MAJOR;
    }

    s_IrcutCtrlFd = open(pDev, O_RDWR);
    if (s_IrcutCtrlFd <= 0)
    {
        mysystem_with_param("mknod %s c %d 0", pDev, iMajor);
        s_IrcutCtrlFd = open(pDev, O_RDWR);
    }

    return s_IrcutCtrlFd;
}

int anj_mw_ircut_set(int status)
{
    int iRet = 0;
    char ircut_mode = (status == IRCUT_DAY) ? 1 : 0;

    if (anj_mw_ircut_open() <= 0)
    {
        return -1;
    }

    if (s_bUseGc615)
    {
        iRet = ioctl(s_IrcutCtrlFd, IRCUT_GC615_IOCTL, (status == IRCUT_DAY) ? 1 : 0);
    }
    else
    {
        iRet = safe_write(s_IrcutCtrlFd, &ircut_mode, 1);
    }

    return iRet;
}

int anj_mw_ircut_init()
{
    const char *pDev = NULL;

    if (s_IrcutCtrlFd > 0)
    {
        __ERR("ircut ctrl fd:%d already exist so close!\n", s_IrcutCtrlFd);
        close(s_IrcutCtrlFd);
        s_IrcutCtrlFd = -1;
    }

    if (anj_mw_ircut_open() <= 0)
    {
        pDev = s_bUseGc615 ? IRCUT_GC615_DEV : IRCUT_IRLED_DEV;
        __ERR("ircut dev %s open failed (gc615_module:%d)\n", pDev, s_bUseGc615);
        return -1;
    }

    __INFO("ircut dev open successful! fd:%d gc615:%d\n", s_IrcutCtrlFd, s_bUseGc615);

    /* 上电先切反向再回 day，保证线圈有动作、软硬件状态一致 */
    anj_mw_ircut_set(IRCUT_NIGHT);
    usleep(350 * 1000);
    anj_mw_ircut_set(IRCUT_DAY);

    return 0;
}

int anj_mw_ircut_uninit()
{
    if (s_IrcutCtrlFd > 0)
    {
        close(s_IrcutCtrlFd);
        s_IrcutCtrlFd = -1;
    }
    s_bUseGc615 = 0;

    return 0;
}
