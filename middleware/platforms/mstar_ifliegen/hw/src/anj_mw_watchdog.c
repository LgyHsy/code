#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <linux/watchdog.h>

#include "anj_mw_comm.h"
#include "anj_mw_watchdog.h"

static int watch_dog_fd = -1;
static int s_stop_feed = 0;

#define WATCH_DOG_FILE "/dev/watchdog"

// #define WATCHDOG_IOCTL_BASE 'W'
// #define WDIOC_SETOPTIONS _IOWR(WATCHDOG_IOCTL_BASE, 4, unsigned int)
// #define WDIOC_KEEPALIVE _IO(WATCHDOG_IOCTL_BASE, 5)
// #define WDIOC_SETTIMEOUT _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
// #define WDIOC_GETTIMEOUT _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define WATCH_DOG_FORCE_RESET 0xF000

int WatchDogOpen(void)
{
    watch_dog_fd = open(WATCH_DOG_FILE, O_RDWR);
    if (watch_dog_fd < 0)
    {
        __ERR("open watch dog %s failed, err=%s\n", WATCH_DOG_FILE, strerror(errno));
        return -1;
    }
    else
    {
        __ERR("open watch dog %s ok, fd=%d\n", WATCH_DOG_FILE, watch_dog_fd);
    }
    int tmo = 60;

    if (ioctl(watch_dog_fd, WDIOC_SETTIMEOUT, &tmo) < 0)
    {
        __ERR("WDIOC_SETTIMEOUT\n");
        return -1;
    }

    tmo = 0;

    if (ioctl(watch_dog_fd, WDIOC_GETTIMEOUT, &tmo) < 0)
    {
        __ERR("WDIOC_GETTIMEOUT\n");
        return -1;
    }

    __ERR("restart in %d secs\n", tmo);

    return 1;
}

int WatchDogFeed(void)
{
    if (watch_dog_fd < 0)
    {
        __ERR("watchdog feed failed, fd=%d\n", watch_dog_fd);
        return 1;
    }

    __LOG_ENTER();
    ioctl(watch_dog_fd, WDIOC_KEEPALIVE, 0);

    return 1;
}

int WatchDogClose(void)
{
    if (watch_dog_fd > 0)
    {
        close(watch_dog_fd);
    }
    watch_dog_fd = -1;

    return 0;
}

int WatchDogForceReset(void)
{
    int data;

    __INFO("WatchDogForceReset\n");

    if (watch_dog_fd < 0)
    {
        WatchDogOpen();
    }

    if (watch_dog_fd < 0)
    {
        return -1;
    }

    __ERR("ioctl watch dog to force reset, fd=%d\n", watch_dog_fd);

    ioctl(watch_dog_fd, WATCH_DOG_FORCE_RESET, &data);
    close(watch_dog_fd);
    watch_dog_fd = -1;

    return 1;
}

int WatchDogSetTimeOut(int timeout, int bStopFeed)
{
    if (watch_dog_fd > 0)
    {
        if (ioctl(watch_dog_fd, WDIOC_SETTIMEOUT, &timeout) < 0)
        {
            __ERR("WDIOC_SETTIMEOUT\n");
            return -1;
        }
    }
    s_stop_feed = bStopFeed;
    return 0;
}

int WatchDogFeedStopGet()
{
    return s_stop_feed;
}