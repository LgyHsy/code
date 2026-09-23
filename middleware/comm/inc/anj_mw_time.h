#ifndef _ANJ_MW_TIME_H_
#define _ANJ_MW_TIME_H_

#include <time.h>
#include <sys/time.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct
{
    unsigned char hour;
    unsigned char minute;
    unsigned char sec;
} DayTime;

typedef struct
{
    DayTime startTime;
    DayTime endTime;
} DayTimeSpan;

#define MAX_DAYTIMESPAN_COUNT 24
#define MAX_WORDDAYTIME_COUNT 7

typedef struct
{
    int workday;
    int timeSpancnt;
    DayTimeSpan timeSpans[MAX_DAYTIMESPAN_COUNT];
} WorkDayTime;

typedef struct
{
    int workdayCnt;
    WorkDayTime workdayTimes[MAX_WORDDAYTIME_COUNT];
} TimeSpanList;

typedef struct
{
    unsigned int workday[MAX_WORDDAYTIME_COUNT]; // 数组0-6标识周日-周六每小时的配置，每个小时占用一个bit位，0-23BIT有效
} TimeSpanCfg;

typedef struct
{
    int year;
    int month;
    int day;
} SYSTEM_DATE;

typedef struct
{
    int bReCreate;      // 重新生成
    int nTargetSeconds; // 目标时长
    SYSTEM_DATE date;
    DayTimeSpan timespan;
} DateTimelapseMsg;

typedef struct
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} SYSTEM_TIME;

int anj_mw_time_getstr(char *pTimeBuf, int pTimeBufLen, time_t iTime);

void anj_mw_rsleep(unsigned int ntime);

unsigned long long anj_mw_get_cputime_ms(struct timespec *pTimespec);

int anj_mw_msecond_to_timespec(struct timespec *pTimespec, unsigned long long iTimeMs);

void GetTime(char *strTime, DayTime *pDayTime);

void GetDayTimeFromStr(char *strTime, DayTime *pDayTime);

void SetAllTimeSpan(TimeSpanCfg *pOutput);

void CheckTimeSpanValid(TimeSpanCfg *pOutput);

int GetDayTimeStr(char *buf, int len, DayTime *pDayTime);

void setTimeSpanList(TimeSpanList *pTimeSpanList, int BeginTime, int EndTime, char *RepeatDays);
void TransTimeSpan2New(const TimeSpanList *pInput, TimeSpanCfg *pOutput);
void TransTimeSpan2Old(const TimeSpanCfg *pInput, TimeSpanList *pOutput);
void setTimeSpanByStr(TimeSpanList *ptimeSpanList, char *timestrategy);
void setTimeSpanByTimeIntervalStr(DayTimeSpan *pDayTimeSpan, char *BeginTime, char *EndTime);

int SystemGetTimeofRun(struct timeval *tv, struct timezone *tz);
int SystemGetRealTime(struct timeval *tv, struct timezone *tz);
void SystemGetNowTime(SYSTEM_TIME *pNowTime);

void SystemLocalTime(struct tm *pLocalTime);

int CheckNowIsInTimeSpan(const TimeSpanCfg *ptimeSpan);

typedef void *(*TimerCallBack)(void *);

typedef struct
{
    int refreshTime;
    TimerCallBack callBackFuc;
} TimerData;

typedef struct _TimerNode
{
    TimerData data;
    struct timeval timeOut;
} TimerNode;

int TimerModuleInit(int *quit);
int AddTimer(TimerData *timer);
int DeleteTimer(TimerData *timer);
int TimerModuleDestory();

void SLEEP_SECOND(int seconds);

/*
    休眠时间
    使用run标记来休眠，把senconds秒的时间分拆为多个时间段来usleep
    如果pRunFlag=NULL，则不判断标记

*/
void SLEEP_SECOND_if_run(int seconds, const int *pRunFlag);
void SLEEP_MSECOND_if_run(int mseconds, const int *pRunFlag);

int isleapyear(int year);

/*获得指定时区指定年月日时分秒的时间戳.*/
/*TZ: 支持分钟, 60*(时区+12)*/
/*tz=(0+12)*60的时候，获取的是0时区指定年月日时分秒格林威治时间的时间戳*/
time_t ch_mktime(int tm_year, int tm_mon, int tm_mday, int tm_hour, int tm_min, int tm_sec, int tz);

/*使用mktime和gmtime函数来获取0时区指定年月日时分秒格林威治时间的时间戳*/
/*mktime函数内部使用TZ环境变量进行时区换算，效率低下很多*/
time_t mktime_utc(int tm_year, int tm_mon, int tm_mday, int tm_hour, int tm_min, int tm_sec);

int GetTimeZoneBySystem();

int GetTimeFromString(char *time_str, struct tm *time);

unsigned int GetCurrentTimeStamp(void);
unsigned long long GetCurrentTimeStampU64(void);

char *debug_time();

#if defined(__cplusplus)
}
#endif

#endif
