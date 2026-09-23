#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mount.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_mw_watchdog.h"
#include "anj_mw_hwctrl.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_module.h"
#include "anjrec_auto_ota.h"
#include "eventhub.h"

int bExit = 0;
int bEnd = 0;

static void sighandel(int sig)
{
    if (!bExit)
    {
        printf("\033[31;1;5m####INFO signal:%d \033[0m\n", sig);
        bExit = 1;
    }
    else
    {
        bEnd = 1;
    }
}

static void init_signals(void)
{
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);

    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGSEGV);
    sa.sa_handler = sighandel;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
}

static int mount_userdata_partition(void)
{
    umount(DATA_BLOCK_MOUNT_PATH);
    if (mount(DATA_BLOCK1, DATA_BLOCK_MOUNT_PATH, "jffs2", 0, NULL) != 0)
    {
        printf("mount %s failed: %s\n", DATA_BLOCK_MOUNT_PATH, strerror(errno));
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    init_signals();
    anj_mw_log_init();
    anj_mw_hwctrl_init();
    mount_userdata_partition();
    eventhub_init();

    if (modules_init() != 0)
    {
        __ERR("recovery modules_init failed\n");
    }
    anjrec_auto_ota_init();

    WatchDogOpen();
    WatchDogFeed();

    __INFO("anjrec recovery started (net + pri + ser)\n");

    while (!bExit)
    {
        if (bEnd)
        {
            break;
        }

        DevInfo *pstDevInfo = getDevInfo();
        if (pstDevInfo != NULL && pstDevInfo->bUpgrading)
        {
            usleep(100 * 1000);
            continue;
        }

        WatchDogFeed();
        sleep(1);
    }

    __INFO("recovery anjrec exit\n");
    anjrec_auto_ota_uninit();
    modules_uninit(NULL);
    eventhub_uninit();
    WatchDogClose();
    anj_mw_log_uninit();
    return 0;
}
