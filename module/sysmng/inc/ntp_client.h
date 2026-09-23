#ifndef __NTP_CLIENT_H__
#define __NTP_CLIENT_H__

//#define PRECISION_SIOCGSTAMP
//#define USE_OBSOLETE_GETTIMEOFDAY
//#define ENABLE_DEBUG

/* when present, debug is a true global */
#ifdef ENABLE_DEBUG
extern int debug;
#else
#define debug 0
#endif

typedef struct  {
	int		tv_sec;		/* seconds */
	int		tv_usec;	/* microseconds */
}MyTimeval;

typedef void (*SetTimeCallback)(int sec, int usec);

void ntp_register_time_callback(SetTimeCallback cb);
void ntp_unregister_time_callback(void);

int ntp_stop_get_time();
int ntp_start_get_time(char* hostname, int cycle_time, int times, int block) ;

#endif

