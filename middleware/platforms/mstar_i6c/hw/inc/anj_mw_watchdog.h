#ifndef _ANJ_MW_WATCHDOG_H_
#define _ANJ_MW_WATCHDOG_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int WatchDogOpen(void);

int WatchDogFeed(void);

int WatchDogClose(void);

int WatchDogForceReset(void);

int WatchDogSetTimeOut(int timeout, int bStopFeed);

int WatchDogFeedStopGet();

#if defined(__cplusplus)
}
#endif

#endif
