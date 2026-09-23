#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_ircut.h"

static int s_IrcutCtrlFd = -1;

int anj_mw_ircut_set(int status)
{
    int iRet = 0;
    char ircut_mode = (status == IRCUT_DAY) ? 1 : 0;

    if (s_IrcutCtrlFd > 0)
    {
        iRet = safe_write(s_IrcutCtrlFd, &ircut_mode, 1);
    }

    return iRet;
}

int anj_mw_ircut_init()
{
    if (s_IrcutCtrlFd > 0)
    {
        __ERR("ircut ctrl fd:%d already exist so close!\n", s_IrcutCtrlFd);
        close(s_IrcutCtrlFd);
        s_IrcutCtrlFd = -1;
    }

    mysystem_with_param("mknod %s c 93 0", ANJ_IRCUT_DEV);

    s_IrcutCtrlFd = open(ANJ_IRCUT_DEV, O_RDWR);
    if (s_IrcutCtrlFd <= 0)
    {
        __ERR("ircut dev open failed\n");
        return -1;
    }

    __INFO("ircut dev open successful! fd:%d\n", s_IrcutCtrlFd);

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

    return 0;
}
