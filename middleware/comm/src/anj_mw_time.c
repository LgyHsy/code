#include <sys/types.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_time.h"
#include "sem_util.h"

#define MAX_TIMER_COUNT 10

static TimerNode g_timer_list[MAX_TIMER_COUNT];
static int g_timer_cnt = 0;
static SemHandle g_sem_handle = NULL;

int anj_mw_time_getstr(char *pTimeBuf, int pTimeBufLen, time_t iTime)
{
    int iRet = -1;
    struct tm stTmNow;
    ANJ_CHK(pTimeBuf != NULL && pTimeBufLen > 0, -1, "input Invalid");

    memset(&stTmNow, 0, sizeof(stTmNow));
    localtime_r(&iTime, &stTmNow);

    snprintf(pTimeBuf, pTimeBufLen, "%02d%s%02d%s%02d %02d:%02d:%02d",
             stTmNow.tm_year + 1900, "-", stTmNow.tm_mon + 1, "-", stTmNow.tm_mday,
             stTmNow.tm_hour, stTmNow.tm_min, stTmNow.tm_sec);
    iRet = 0;
endFunc:
    return iRet;
}

void anj_mw_rsleep(unsigned int ntime)
{
    fd_set rfds;
    struct timeval tv;

    /* Watch stdin  (fd 0) to see when it has input. */
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    /* Wait up to five seconds. */
    tv.tv_sec = ntime / 1000000;
    tv.tv_usec = ntime % 1000000;
    select(0, &rfds, NULL, NULL, &tv);
}

unsigned long long anj_mw_get_cputime_ms(struct timespec *pTimespec)
{
    struct timespec ts;
    if (NULL == pTimespec)
    {
        pTimespec = &ts;
    }
    if (0 > clock_gettime(CLOCK_MONOTONIC, pTimespec))
    {
        //__ERR("clock gettime err: %s\n", strerror(errno));
        return 0;
    }
    return (unsigned long long)pTimespec->tv_sec * 1000 + (unsigned long long)pTimespec->tv_nsec / (1000 * 1000);
}

int anj_mw_msecond_to_timespec(struct timespec *pTimespec, unsigned long long iTimeMs)
{
    if (NULL == pTimespec)
    {
        __ERR("Input null\n");
        return -1;
    }

    pTimespec->tv_sec = iTimeMs / 1000;
    pTimespec->tv_nsec = (iTimeMs % 1000) * 1000 * 1000;
    return 0;
}

// strTime format : xx:xx:xx
void GetTime(char *strTime, DayTime *pDayTime)
{
    if (strTime == NULL)
    {
        __ERR("time string is null\n");
        pDayTime->hour = 0;
        pDayTime->minute = 0;
        pDayTime->sec = 0;
        return;
    }

    int index = 0;
    char *token = strtok(strTime, ":");
    while (token != NULL && index < 3)
    {
        if (index == 0)
            pDayTime->hour = atoi(token);
        else if (index == 1)
            pDayTime->minute = atoi(token);
        else if (index == 2)
            pDayTime->sec = atoi(token);
        index++;
        token = strtok(NULL, ":");
    }
    __INFO("%d:%d:%d\n", pDayTime->hour, pDayTime->minute, pDayTime->sec);
}

void GetDayTimeFromStr(char *strTime, DayTime *pDayTime)
{
    GetTime(strTime, pDayTime);
}

void SetAllTimeSpan(TimeSpanCfg *pOutput)
{
    for (int i = 0; i < MAX_WORDDAYTIME_COUNT; i++)
    {
        pOutput->workday[i] = 0xffffff;
    }
}

void CheckTimeSpanValid(TimeSpanCfg *pOutput)
{
    int bAllZero = 1;
    for (int i = 0; i < MAX_WORDDAYTIME_COUNT; i++)
    {
        if (pOutput->workday[i] != 0)
        {
            bAllZero = 0;
            break;
        }
    }

    if (bAllZero > 0)
    {
        SetAllTimeSpan(pOutput);
    }
}

int GetDayTimeStr(char *buf, int len, DayTime *pDayTime)
{
    if (pDayTime->hour > 24 || pDayTime->hour < 0 ||
        pDayTime->minute > 59 || pDayTime->minute < 0 ||
        pDayTime->sec > 59 || pDayTime->sec < 0 ||
        (pDayTime->hour == 24 && pDayTime->minute > 0))
    {
        memset(pDayTime, 0, sizeof(DayTime));
    }

    memset(buf, '\0', len);
    return snprintf(buf, len, "%2d:%2d:%2d", pDayTime->hour,
                    pDayTime->minute, pDayTime->sec);
}

void setTimeSpanList(TimeSpanList *pTimeSpanList, int BeginTime, int EndTime, char *RepeatDays)
{
    if (!pTimeSpanList || !RepeatDays)
        return;

    int i = 0;
    int dayindex[7] = {0};
    char daystrlist[7][8] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

    int timeSpanIndex = 0;

    bool bhavetime = false;

    for (i = 0; i < 7; i++)
    {
        if (strstr(RepeatDays, daystrlist[i]))
        {
            dayindex[i] = 1;
        }
    }

    pTimeSpanList->workdayCnt = 7;
    for (i = 0; i < 7 && i < MAX_WORDDAYTIME_COUNT; i++)
    {
        bhavetime = false;

        if (dayindex[i] == 0)
        {
            // printf("i:%d skip \n", i);
            continue;
        }

        int timeSpancnt = pTimeSpanList->workdayTimes[i].timeSpancnt;
        pTimeSpanList->workdayTimes[i].workday = i;

        /* check */
        for (timeSpanIndex = 0; timeSpanIndex < timeSpancnt; timeSpanIndex++)
        {
            int BeginTimetmp, EndTimetmp;
            DayTimeSpan *pDayTimeSpan = &(pTimeSpanList->workdayTimes[i].timeSpans[timeSpanIndex]);

            BeginTimetmp = pDayTimeSpan->startTime.hour * 3600 + pDayTimeSpan->startTime.minute * 60 + pDayTimeSpan->startTime.sec;
            EndTimetmp = pDayTimeSpan->endTime.hour * 3600 + pDayTimeSpan->endTime.minute * 60 + pDayTimeSpan->endTime.sec;

            if ((BeginTimetmp == BeginTime) || (EndTimetmp == EndTime))
            {
                // __INFO("i:%d havetime skip BeginTimetmp:%d[BeginTime:%d] EndTimetmp:%d[EndTime:%d] \n", i, BeginTimetmp, BeginTime, EndTimetmp, EndTime);
                bhavetime = true;
                break;
            }
            else if ((BeginTimetmp < BeginTime && EndTimetmp > BeginTime) || (BeginTimetmp < EndTime && EndTimetmp > EndTime))
            {
                // __INFO("i:%d havetime skip BeginTimetmp:%d[BeginTime:%d] EndTimetmp:%d[EndTime:%d] \n", i, BeginTimetmp, BeginTime, EndTimetmp, EndTime);
                bhavetime = true;
                break;
            }
        }

        if (bhavetime)
            continue;

        if (timeSpancnt < MAX_DAYTIMESPAN_COUNT)
        {
            // __INFO("i:%d BeginTime:%d EndTime:%d timeSpancnt:%d  \n", i, BeginTime, EndTime, pTimeSpanList->workdayTimes[i].timeSpancnt);
            DayTimeSpan *pDayTimeSpan = &(pTimeSpanList->workdayTimes[i].timeSpans[timeSpancnt]);

            pDayTimeSpan->startTime.hour = BeginTime / 3600;
            pDayTimeSpan->startTime.minute = (BeginTime % 3600) / 60;
            pDayTimeSpan->startTime.sec = (BeginTime % 3600) % 60;

            pDayTimeSpan->endTime.hour = EndTime / 3600;
            pDayTimeSpan->endTime.minute = (EndTime % 3600) / 60;
            pDayTimeSpan->endTime.sec = (EndTime % 3600) % 60;

            pTimeSpanList->workdayTimes[i].timeSpancnt++;
        }
    }
}

void TransTimeSpan2New(const TimeSpanList *pInput, TimeSpanCfg *pOutput)
{
    memset(pOutput, 0, sizeof(TimeSpanCfg));

    int iIndex = 0;
    for (iIndex = 0; iIndex < MAX_WORDDAYTIME_COUNT && iIndex < pInput->workdayCnt; iIndex++)
    {
        int workday = pInput->workdayTimes[iIndex].workday;
        if (workday > 7 || workday < 0)
            continue;

        int timespan = 0;
        for (timespan = 0; timespan < MAX_DAYTIMESPAN_COUNT && timespan < pInput->workdayTimes[iIndex].timeSpancnt; timespan++)
        {
            int startHour = pInput->workdayTimes[iIndex].timeSpans[timespan].startTime.hour;
            int startMin = pInput->workdayTimes[iIndex].timeSpans[timespan].startTime.minute;
            int stopHour = pInput->workdayTimes[iIndex].timeSpans[timespan].endTime.hour;
            int stopMin = pInput->workdayTimes[iIndex].timeSpans[timespan].endTime.minute;
            if (startHour > 24 || stopHour > 24)
                continue;

            if (startMin > 59)
                startHour++;
            if (stopMin > 0)
                stopHour++;

            //			__ERR("workday:%d, timespan:%d, %d-%d", workday, timespan, startHour, stopHour);

            int jIndex = 0;
            for (jIndex = startHour; jIndex < stopHour && jIndex < 24; jIndex++)
            {
                if (workday == 7) // 配置是每天，所以同样配置需要把周日-周六都设置一遍
                {
                    int kIndex = 0;
                    for (kIndex = 0; kIndex < MAX_WORDDAYTIME_COUNT; kIndex++)
                    {
                        BIT_SET_32(pOutput->workday[kIndex], jIndex);
                    }
                }
                else // 配置是单天，只设置相应工作日
                {
                    BIT_SET_32(pOutput->workday[workday], jIndex);
                }
            }
        }
    }

    CheckTimeSpanValid(pOutput);
}

void TransTimeSpan2Old(const TimeSpanCfg *pInput, TimeSpanList *pOutput)
{
    memset(pOutput, 0, sizeof(TimeSpanList));
    int iIndex = 0;

    int bAllSet = 1;
    for (iIndex = 0; iIndex < MAX_WORDDAYTIME_COUNT; iIndex++)
    {
        if ((pInput->workday[iIndex] & 0xffffff) != 0xffffff)
        {
            __ERR("workday %d: %#x, bAllSet=0\n", iIndex, pInput->workday[iIndex]);
            bAllSet = 0;
            break;
        }
        else
        {
            //			__ERR("workday %d: %#x\n", iIndex, pInput->workday[iIndex] );
        }
    }

    // 全部设置
    if (bAllSet > 0)
    {
        pOutput->workdayCnt = 1;
        pOutput->workdayTimes[0].workday = 7;
        pOutput->workdayTimes[0].timeSpancnt = 1;
        pOutput->workdayTimes[0].timeSpans[0].endTime.hour = 23;
        pOutput->workdayTimes[0].timeSpans[0].endTime.minute = 59;
        return;
    }

    for (iIndex = 0; iIndex < MAX_WORDDAYTIME_COUNT; iIndex++)
    {
        int workday = pOutput->workdayCnt;
        WorkDayTime *pWorkDayTime = &pOutput->workdayTimes[workday];
        pWorkDayTime->workday = iIndex;
        if ((pInput->workday[iIndex] & 0xffffff) == 0xffffff)
        {
            //			__ERR("workday %d: %#x\n", iIndex, pInput->workday[iIndex] );

            pWorkDayTime->timeSpancnt = 1;
            pWorkDayTime->timeSpans[0].endTime.hour = 23;
            pWorkDayTime->timeSpans[0].endTime.minute = 59;
        }
        else
        {
            int fromHour = -1;
            int EndHour = -1;
            int jIndex = 0;

            pWorkDayTime->timeSpancnt = 0;
            for (jIndex = 0; jIndex < 24; jIndex++)
            {
                if (BIT_GET_32(pInput->workday[iIndex], jIndex) > 0)
                {
                    if (fromHour < 0)
                    {
                        fromHour = jIndex;
                    }

                    EndHour = jIndex;
                    if (jIndex == 23) // 最后一个小时了，需要处理
                    {
                        int timeSpancnt = pWorkDayTime->timeSpancnt;
                        pWorkDayTime->timeSpans[timeSpancnt].startTime.hour = fromHour;
                        pWorkDayTime->timeSpans[timeSpancnt].startTime.minute = 0;
                        pWorkDayTime->timeSpans[timeSpancnt].endTime.hour = EndHour;
                        pWorkDayTime->timeSpans[timeSpancnt].endTime.minute = 59;
                        pWorkDayTime->timeSpancnt++;
                    }
                }
                else
                { // 碰到了间断的时间点，先设置上一段
                    if (fromHour >= 0 && EndHour >= 0)
                    {
                        int timeSpancnt = pWorkDayTime->timeSpancnt;
                        pWorkDayTime->timeSpans[timeSpancnt].startTime.hour = fromHour;
                        pWorkDayTime->timeSpans[timeSpancnt].startTime.minute = 0;
                        pWorkDayTime->timeSpans[timeSpancnt].endTime.hour = EndHour;
                        pWorkDayTime->timeSpans[timeSpancnt].endTime.minute = 59;
                        pWorkDayTime->timeSpancnt++;
                    }

                    fromHour = -1;
                    EndHour = -1;
                }
            }
        }

        if (pWorkDayTime->timeSpancnt > 0)
            pOutput->workdayCnt++;
    }
}

void setTimeSpanByStr(TimeSpanList *ptimeSpanList, char *timestrategy)
{
    if (ptimeSpanList == NULL || timestrategy == NULL)
        return;

    char *token;
    char *saveptr;
    int day_index = 0;

    token = strtok_r(timestrategy, ",", &saveptr);
    while (token != NULL && day_index < 7) // Limit to 7 days
    {
        int bgetimeSpan = 0;
        int index = 0;
        int preindex = -1;
        int timeSpancnt = 0;
        DayTimeSpan timeSpans[MAX_DAYTIMESPAN_COUNT];
        const char *str = NULL;
        int64_t ialarmtime;

        str = strstr(token, ":");
        if (str == NULL)
        {
            token = strtok_r(NULL, ",", &saveptr);
            day_index++;
            continue;
        }

        sscanf(str + 1, "%lld", &ialarmtime);

        memset(timeSpans, 0, sizeof(timeSpans));
        while (1)
        {
            if (index >= 24)
            {
                if (bgetimeSpan == 1)
                {
                    timeSpans[timeSpancnt].startTime.hour = preindex;
                    timeSpans[timeSpancnt].endTime.hour = 23;
                    timeSpans[timeSpancnt].endTime.minute = 59;
                    timeSpans[timeSpancnt].endTime.sec = 59;

                    timeSpancnt++;
                    if (timeSpancnt >= MAX_DAYTIMESPAN_COUNT)
                        timeSpancnt = MAX_DAYTIMESPAN_COUNT - 1;
                }
                break;
            }

            if (bgetimeSpan == 0 && ((ialarmtime >> index) & 0x01))
            {
                bgetimeSpan = 1;
                preindex = index;
            }

            if (bgetimeSpan == 1 && ((ialarmtime >> index) & 0x01) == 0)
            {
                timeSpans[timeSpancnt].startTime.hour = preindex;
                timeSpans[timeSpancnt].endTime.hour = index;

                bgetimeSpan = 0;
                timeSpancnt++;
                if (timeSpancnt >= MAX_DAYTIMESPAN_COUNT)
                    break;
            }

            index++;
        }

        ptimeSpanList->workdayTimes[ptimeSpanList->workdayCnt].workday = day_index;
        ptimeSpanList->workdayTimes[ptimeSpanList->workdayCnt].timeSpancnt = timeSpancnt;
        memcpy(ptimeSpanList->workdayTimes[ptimeSpanList->workdayCnt].timeSpans, timeSpans, sizeof(timeSpans));
        ptimeSpanList->workdayCnt++;

        token = strtok_r(NULL, ",", &saveptr);
        day_index++;
    }
}

void setTimeSpanByTimeIntervalStr(DayTimeSpan *pDayTimeSpan, char *BeginTime, char *EndTime)
{
    if (pDayTimeSpan == NULL || BeginTime == NULL || EndTime == NULL)
        return;

    int hour = 0, min = 0, sec = 0;

    sscanf(BeginTime, "%d:%d:%d", &hour, &min, &sec);
    pDayTimeSpan->startTime.hour = hour;
    pDayTimeSpan->startTime.minute = min;
    pDayTimeSpan->startTime.sec = sec;

    sscanf(EndTime, "%d:%d:%d", &hour, &min, &sec);
    pDayTimeSpan->endTime.hour = hour;
    pDayTimeSpan->endTime.minute = min;
    pDayTimeSpan->endTime.sec = sec;
}

int SystemGetTimeofRun(struct timeval *tv, struct timezone *tz)
{
#if GET_SYSTEM_TIME_RELATIVE
    struct timespec runtime;
    if (0 > clock_gettime(CLOCK_MONOTONIC, &runtime))
    {
        __ERR("clock gettime err: %s\n", strerror(errno));
        return -1;
    }

    tv->tv_sec = runtime.tv_sec;
    tv->tv_usec = runtime.tv_nsec / 1000;
#else
    gettimeofday(tv, tz);
#endif
    return 0;
}

void SystemLocalTime(struct tm *pLocalTime)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, pLocalTime);
}

void SystemGetNowTime(SYSTEM_TIME *pNowTime)
{
    if (NULL == pNowTime)
    {
        return;
    }

    struct timeval tv;
    struct tm *ptm, tbuf;
    gettimeofday(&tv, NULL);
    ptm = localtime_r(&tv.tv_sec, &tbuf);

    pNowTime->year      = 1900 + ptm->tm_year;
    pNowTime->month     = 1 + ptm->tm_mon;
    pNowTime->day       = ptm->tm_mday;
    pNowTime->hour      = ptm->tm_hour;
    pNowTime->minute    = ptm->tm_min;
    pNowTime->second    = ptm->tm_sec;
}

int CheckNowIsInTimeSpan(const TimeSpanCfg *ptimeSpan)
{
    struct tm ptm;
    int workday_now, hour_now;

    SystemLocalTime(&ptm);

    workday_now = ptm.tm_wday;
    hour_now = ptm.tm_hour;
    if (BIT_GET_32(ptimeSpan->workday[workday_now], hour_now))
        return 1;
    else
        return 0;
}

int TimevalCompare(struct timeval tv1, struct timeval tv2)
{
    if (tv1.tv_sec > tv2.tv_sec)
    {
        return 1;
    }
    else if (tv1.tv_sec == tv2.tv_sec)
    {
        if (tv1.tv_usec > tv2.tv_usec)
        {
            return 1;
        }
        else if (tv1.tv_usec == tv2.tv_usec)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

void *CheckTimeThr(void *arg)
{
    struct timezone tz;
    struct timeval tv;

    int *quit = (int *)arg;

    while (!(*quit))
    {
        SemWait(g_sem_handle);

        int i;
        for (i = 0; i < g_timer_cnt && i < MAX_TIMER_COUNT; i++)
        {
            SystemGetTimeofRun(&tv, &tz);
            if (TimevalCompare(tv, g_timer_list[i].timeOut) >= 0)
            {
                void *arg = NULL;
                (*g_timer_list[i].data.callBackFuc)(arg);
                tv.tv_sec += g_timer_list[i].data.refreshTime;
                memcpy(&(g_timer_list[i].timeOut), &tv, sizeof(tv));
            }
        }

        SemRelease(g_sem_handle);

        //__ERR("timer loop.\n");

        SLEEP_SECOND(5);
    }

    return NULL;
}

static int CreateCheckTimeThr(int *quit)
{
    int ret;
    pthread_t id;

    ret = pthread_create(&id, 0, CheckTimeThr, (void *)quit);
    if (ret != 0)
    {
        __ERR("create ethread fail(%s)\n", strerror(errno));
    }

    return ret;
}

int TimerModuleInit(int *quit)
{
    int ret;

    if (g_sem_handle == NULL)
        g_sem_handle = SemCreate();

    if (g_sem_handle == NULL)
    {
        return -1;
    }

    ret = CreateCheckTimeThr(quit);

    return ret;
}

int TimerModuleDestory()
{
    SemDestroy(g_sem_handle);
    g_sem_handle = NULL;

    return 0;
}

int AddTimer(TimerData *timer)
{
    if (g_sem_handle == NULL)
        return -1;

    SemWait(g_sem_handle);

    TimerNode *timerNode = (TimerNode *)malloc(sizeof(TimerNode));
    memset(timerNode, 0, sizeof(TimerNode));

    memcpy(&(timerNode->data), timer, sizeof(TimerData));

    struct timezone tz;
    struct timeval tv;

    SystemGetTimeofRun(&tv, &tz);
    tv.tv_sec += timer->refreshTime;
    memcpy(&(timerNode->timeOut), &tv, sizeof(tv));

    int i;
    for (i = 0; i < g_timer_cnt && i < MAX_TIMER_COUNT; i++)
    {
        if (g_timer_list[i].data.callBackFuc == timerNode->data.callBackFuc)
        {
            memcpy(&(g_timer_list[i]), timerNode, sizeof(TimerNode));
            free(timerNode);
            SemRelease(g_sem_handle);
            return 0;
        }
    }

    if (g_timer_cnt >= MAX_TIMER_COUNT)
    {
        free(timerNode);
        SemRelease(g_sem_handle);
        __ERR("too many timer\n");
        return -1;
    }

    memcpy(&(g_timer_list[g_timer_cnt]), timerNode, sizeof(TimerNode));
    g_timer_cnt++;

    free(timerNode);
    SemRelease(g_sem_handle);

    return 0;
}

int DeleteTimer(TimerData *timer)
{
    if (g_sem_handle == NULL)
        return -1;

    SemWait(g_sem_handle);

    int i;
    int find = 0;
    for (i = 0; i < g_timer_cnt && i < MAX_TIMER_COUNT; i++)
    {
        if (g_timer_list[i].data.callBackFuc == timer->callBackFuc)
        {
            find = 1;
            break;
        }
    }

    int idx = i;
    if (find)
    {
        if (idx != (g_timer_cnt - 1))
        {
            for (; idx < g_timer_cnt && idx < MAX_TIMER_COUNT; idx++)
            {
                memcpy(&(g_timer_list[idx]), &(g_timer_list[idx + 1]), sizeof(TimerNode));
            }
        }
        else
        {
            memset(&(g_timer_list[idx]), 0, sizeof(TimerNode));
        }

        g_timer_cnt--;
    }

    SemRelease(g_sem_handle);
    return 0;
}

void ResetTimerList()
{
    SemWait(g_sem_handle);

    int i;
    struct timezone tz;
    struct timeval tv;

    for (i = 0; i < g_timer_cnt && i < MAX_TIMER_COUNT; i++)
    {
        SystemGetTimeofRun(&tv, &tz);
        tv.tv_sec += g_timer_list[i].data.refreshTime;
        memcpy(&(g_timer_list[i].timeOut), &tv, sizeof(tv));
    }

    SemRelease(g_sem_handle);
}

void SLEEP_SECOND(int seconds)
{
    usleep((seconds) * 1000 * 1000);
}

void SLEEP_SECOND_if_run(int seconds, const int *pRunFlag)
{
    if (pRunFlag == NULL)
    {
        usleep(seconds * 1000 * 1000);
        return;
    }

    int useconds = (100000); // 100ms执行一次，避免CPU过高
    int nTimes = 0;
    int nTotalTimes = (seconds * 1000 * 1000) / useconds;
    while (*pRunFlag && nTimes++ < nTotalTimes)
    {
        usleep(useconds);
    }
    return;
}

void SLEEP_MSECOND_if_run(int mseconds, const int *pRunFlag)
{
    if (pRunFlag == NULL || mseconds < 100)
    {
        usleep(mseconds * 1000);
        return;
    }

    int useconds = (100000); // 100ms执行一次，避免CPU过高
    int nTimes = 0;
    int nTotalTimes = (mseconds * 1000) / useconds;
    while (*pRunFlag && nTimes++ < nTotalTimes)
    {
        usleep(useconds);
    }
    return;
}

int isleapyear(int year)
{
    /*
    1.普通情况求闰年只需除以4可除尽即可
    2.如果是100的倍数但不是400的倍数,那就不是闰年了,即末两位都是零的整除400才行 像1700、1800、1900、2100都不是闰年,但是2000、2400是的。
    */
    if (year % 400 == 0)
        return 1;
    if (year % 100 == 0)
        return 0;
    if (year % 4 == 0)
        return 1;

    return 0;
}

time_t ch_mktime(int tm_year, int tm_mon, int tm_mday, int tm_hour, int tm_min, int tm_sec, int tz)
{
    const int mon_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    long tyears, tdays, leap_years, utc_hrs;
    int is_leap = 0;
    int i, ryear;

    // 判断闰年
    ryear = tm_year;
    is_leap = (((ryear % 100 != 0) && (ryear % 4 == 0)) || (ryear % 400 == 0)) ? 1 : 0;
    is_leap = isleapyear(ryear);

    tyears = tm_year - 1970; // 时间戳从1970年开始算起
    tm_mon -= 1;
    if (tm_mon < 2 && is_leap == 1)
    {
        leap_years = (tyears + 2) / 4 - 1; // 1970年不是闰年，从1972年开始闰年
                                           // 闰年的月份小于1，需要减去一天
    }
    else
    {
        leap_years = (tyears + 2) / 4;
    }

    tdays = 0;
    for (i = 0; i < tm_mon; ++i)
    {
        tdays += mon_days[i];
    }
    tdays += tm_mday - 1; // 减去今天
    tdays += tyears * 365 + leap_years;
    utc_hrs = tm_hour;

    time_t ret = (tdays * 86400) + (utc_hrs * 3600) + (tm_min * 60) + tm_sec;
    ret -= (tz - 60 * 12) * 60;

    return ret;
}

time_t mktime_utc(int tm_year, int tm_mon, int tm_mday, int tm_hour, int tm_min, int tm_sec)
{
    time_t time_seconds = 0;
    struct tm tm_local;
    memset(&tm_local, 0, sizeof(tm_local));
    tm_local.tm_year = tm_year - 1900;
    tm_local.tm_mon = tm_mon - 1;
    tm_local.tm_mday = tm_mday;
    tm_local.tm_hour = tm_hour;
    tm_local.tm_min = tm_min;
    tm_local.tm_sec = tm_sec;
    time_seconds = mktime(&tm_local); // 本地时间这个点的秒数

    // 将这个时间点转换成UTC 0时间的年月日时分秒
    // 以东8区为例，得到的年月日时分秒实际上比本地时间早了8小时
    struct tm tm_gmt;
    gmtime_r(&time_seconds, &tm_gmt);

    // 重新计算UTC年月日时分秒的秒数，mktime是以本地时间计算的
    time_t time_seconds_gm = mktime(&tm_gmt);

    // 计算这个时间差
    long long diff = (long long)time_seconds - (long long)time_seconds_gm;

    // 将这个时间差加上本地时间，就是UTC 0时区实际上跑到指定的年月日的时间戳
    time_seconds_gm = (time_t)((long long)time_seconds + (long long)diff);

    //	printf("|%20s|%20lld|\n", "time_seconds", (long long)time_seconds);
    //	printf("|%20s|%20lld|\n", "time_seconds_gm", (long long)time_seconds_gm);

    return time_seconds_gm;
}

// 将-12~+12时区换算到0-1440的偏移分钟
int GetTimeZoneBySystem()
{
    struct tm *ptm, tbuf;
    time_t now = time(NULL);
    ptm = gmtime_r(&now, &tbuf); // 得到0时区的tm结构，比localtime少偏移秒数

    time_t utc = mktime(ptm);

    time_t diff = now - utc;
    int timezone = (int)(12 * 60) + (int)(diff / 60);
    __INFO("%lld-%lld=%lld, timezone=%d\n", (long long)utc, (long long)now, (long long)diff, timezone);

    return timezone;
}

int GetTimeFromString(char *time_str, struct tm *time)
{
    char *sample_time = (char *)"20090101 00:00:00";

    if (time_str == NULL)
    {
        __ERR("time_str=NULL!\n");
        return -1;
    }

    __INFO("time_str=%s\n", time_str);

    if (strlen(time_str) != strlen(sample_time))
    {
        __ERR("time str: %s length error\n", time_str);
        return -1;
    }

    char year[5];
    char month[3];
    char day[3];
    char hour[3];
    char minute[3];
    char second[3];

    memcpy(year, time_str, 4);
    year[4] = '\0';

    memcpy(month, time_str + strlen("2009"), 2);
    month[2] = '\0';

    memcpy(day, time_str + strlen("200901"), 2);
    day[2] = '\0';

    memcpy(hour, time_str + strlen("20090101 "), 2);
    hour[2] = '\0';

    memcpy(minute, time_str + strlen("20090101 00:"), 2);
    minute[2] = '\0';

    memcpy(second, time_str + strlen("20090101 00:00:"), 2);
    second[2] = '\0';

    __INFO("year=%s, month=%s, day=%s, hour=%s, minute=%s, second=%s\n",
           year, month, day, hour, minute, second);

    memset(time, 0, sizeof(struct tm));

    time->tm_year = atoi(year) - 1900;
    time->tm_mon = atoi(month) - 1;
    time->tm_mday = atoi(day);
    time->tm_hour = atoi(hour);
    time->tm_min = atoi(minute);
    time->tm_sec = atoi(second);

    return 1;
}

unsigned int GetCurrentTimeStamp(void)
{
    unsigned long long mSeconds = 0;
    if (ANJ_PROJECT_TYPE == PROJECT_TYPE_AOV)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mSeconds = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
    else
    {
        struct timespec runtime;
        if (0 > clock_gettime(CLOCK_MONOTONIC, &runtime))
        {
            __ERR("clock gettime err: %s\n", strerror(errno));
            return 0;
        }

        unsigned long long part_tv_sec = (unsigned long long)runtime.tv_sec * 1000;
        unsigned long long part_tv_nsec = (unsigned long long)runtime.tv_nsec / 1000000;
        mSeconds = part_tv_sec + part_tv_nsec;
    }

    return (unsigned int)(mSeconds & 0xffffffff);
}

unsigned long long GetCurrentTimeStampU64(void)
{
    unsigned long long mSeconds = 0;
#if GET_SYSTEM_TIME_RELATIVE
    struct timespec runtime;
    if (0 > clock_gettime(CLOCK_MONOTONIC, &runtime))
    {
        __ERR("clock gettime err: %s\n", strerror(errno));
        return 0;
    }

    unsigned long long part_tv_sec = (unsigned long long)runtime.tv_sec * 1000;
    unsigned long long part_tv_nsec = (unsigned long long)runtime.tv_nsec / 1000000;
    mSeconds = part_tv_sec + part_tv_nsec;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    mSeconds = tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif

    return mSeconds;
}

char *debug_time()
{
    static char str[64] = {0};
    struct tm *t, tbuf;
    time_t tsec = (time_t)time(0);

    t = localtime_r(&tsec, &tbuf);

    int ret = snprintf(str, sizeof(str), "%04d-%02d-%02d %02d:%02d:%02d",
                       2000 + t->tm_year - 100, t->tm_mon + 1,
                       t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    if (ret < 0)
    {
        memset(str, 0, sizeof(str));
    }
    return str;
}
