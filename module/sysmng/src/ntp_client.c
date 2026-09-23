/*
* ntpclient.c - NTP client
*
* Copyright (C) 1997, 1999, 2000, 2003, 2006, 2007, 2010, 2015  Larry Doolittle  <larry@doolittle.boa.org>
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License (Version 2,
*  June 1991) as published by the Free Software Foundation.  At the
*  time of writing, that license was published by the FSF with the URL
*  http://www.gnu.org/copyleft/gpl.html, and is incorporated herein by
*  reference.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  Possible future improvements:
*      - Write more documentation  :-(
*      - Support leap second processing
*      - Support IPv6
*      - Support multiple (interleaved) servers
*
*  Compile with -DPRECISION_SIOCGSTAMP if your machine really has it.
*  Older kernels (before the tickless era, pre 3.0?) only give an answer
*  to the nearest jiffy (1/100 second), not so interesting for us.
*
*  If the compile gives you any flak, check below in the section
*  labelled "XXX fixme - non-automatic build configuration".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>     /* gethostbyname */
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#ifdef PRECISION_SIOCGSTAMP
#include <sys/ioctl.h>
#endif
#ifdef USE_OBSOLETE_GETTIMEOFDAY
#include <sys/time.h>
#endif

#include "ntp_client.h"
#include <sys/prctl.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"


/* Default to the RFC-4330 specified value */
#ifndef MIN_INTERVAL
#define MIN_INTERVAL 15
#endif

#ifdef ENABLE_DEBUG
#define DEBUG_OPTION "d"
int debug=1;
#else
#define DEBUG_OPTION
#endif

#ifdef ENABLE_REPLAY
#define  REPLAY_OPTION   "r"
#else
#define  REPLAY_OPTION
#endif

#include <stdint.h>
typedef uint32_t u32;  /* universal for C99 */
/* typedef u_int32_t u32;   older Linux installs? */

/* XXX fixme - non-automatic build configuration */
#ifdef __linux__
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/timex.h>
#include <netdb.h>
#else
extern struct hostent *gethostbyname(const char *name);
extern int h_errno;
#define herror(hostname) \
    fprintf(stderr,"Error %d looking up hostname %s\n", h_errno,hostname)
#endif
/* end configuration for host systems */

#define JAN_1970        0x83aa7e80      /* 2208988800 1970 - 1900 in seconds */
#define NTP_PORT (123)

/* How to multiply by 4294.967296 quickly (and not quite exactly)
* without using floating point or greater than 32-bit integers.
* If you want to fix the last 12 microseconds of error, add in
* (2911*(x))>>28)
*/
#define NTPFRAC(x) ( 4294*(x) + ( (1981*(x))>>11 ) )

/* The reverse of the above, needed if we want to set our microsecond
* clock (via clock_settime) based on the incoming time in NTP format.
* Basically exact.
*/
#define USEC(x) ( ( (x) >> 12 ) - 759 * ( ( ( (x) >> 10 ) + 32768 ) >> 16 ) )

/* Converts NTP delay and dispersion, apparently in seconds scaled
* by 65536, to microseconds.  RFC-1305 states this time is in seconds,
* doesn't mention the scaling.
* Should somehow be the same as 1000000 * x / 65536
*/
#define sec2u(x) ( (x) * 15.2587890625 )

struct ntptime 
{
    unsigned int coarse;
    unsigned int fine;
};

struct ntp_control 
{
    u32 time_of_send[2];
    int live;
    int set_clock;   /* non-zero presumably needs root privs */
    int probe_count;
    int cycle_time;
    int goodness;
    int cross_check;
    char hostname[64];
    char serv_addr[4];
    anj_thread_s ntp_thread;
};

static struct ntp_control ntpc;

/* prototypes for some local routines */
static void send_packet(int usd, u32 time_sent[2]);
static MyTimeval rfc1305print(u32 *data, struct ntptime *arrival, struct ntp_control *ntpc, int *error);

static void ntpc_gettime(u32 *time_coarse, u32 *time_fine)
{
#ifndef USE_OBSOLETE_GETTIMEOFDAY
    /* POSIX 1003.1-2001 way to get the system time
    */
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    *time_coarse = now.tv_sec + JAN_1970;
    *time_fine   = NTPFRAC(now.tv_nsec/1000);
#else
    /* Traditional Linux way to get the system time
    */
    MyTimeval now;
    gettimeofday(&now, NULL);
    *time_coarse = now.tv_sec + JAN_1970;
    *time_fine   = NTPFRAC(now.tv_usec);
#endif
}

static void send_packet(int usd, u32 time_sent[2])
{
    u32 data[12];
#define LI 0
#define VN 3
#define MODE 3
#define STRATUM 0
#define POLL 4
#define PREC -6

    if (debug) 
        fprintf(stderr,"Sending ...\n");

    if (sizeof data != 48) 
    {
        fprintf(stderr,"size error\n");
        return;
    }

    memset(data, 0, sizeof(data));
    data[0] = htonl (
        ( LI << 30 ) | ( VN << 27 ) | ( MODE << 24 ) |
        ( STRATUM << 16) | ( POLL << 8 ) | ( PREC & 0xff ) );
    data[1] = htonl(1<<16);  /* Root Delay (seconds) */
    data[2] = htonl(1<<16);  /* Root Dispersion (seconds) */
    ntpc_gettime(time_sent, time_sent+1);
    data[10] = htonl(time_sent[0]); /* Transmit Timestamp coarse */
    data[11] = htonl(time_sent[1]); /* Transmit Timestamp fine   */
    send(usd,data,48,0);
}

static void get_packet_timestamp(int usd, struct ntptime *udp_arrival_ntp)
{
#ifdef PRECISION_SIOCGSTAMP
    MyTimeval udp_arrival;
    if (ioctl(usd, SIOCGSTAMP, &udp_arrival) < 0) 
    {
        perror("ioctl-SIOCGSTAMP");
        ntpc_gettime(&udp_arrival_ntp->coarse, &udp_arrival_ntp->fine);
    } 
    else
    {
        udp_arrival_ntp->coarse = udp_arrival.tv_sec + JAN_1970;
        udp_arrival_ntp->fine   = NTPFRAC(udp_arrival.tv_usec);
    }
#else
    (void) usd;  /* not used */
    ntpc_gettime(&udp_arrival_ntp->coarse, &udp_arrival_ntp->fine);
#endif
}

static int check_source(int data_len, struct sockaddr_in *sa_in, unsigned int sa_len, struct ntp_control *ntpc)
{
    struct sockaddr *sa_source = (struct sockaddr *) sa_in;
    (void) sa_len;  /* not used */
    if (debug) 
    {
        printf("packet of length %d received\n",data_len);
        if (sa_source->sa_family == AF_INET) 
        {
            printf("Source: INET Port %d host %s\n",
                ntohs(sa_in->sin_port),inet_ntoa(sa_in->sin_addr));
        } 
        else
        {
            printf("Source: Address family %d\n",sa_source->sa_family);
        }
    }
    /* we could check that the source is the server we expect, but
    * Denys Vlasenko recommends against it: multihomed hosts get it
    * wrong too often. */
#if 0
    if (memcmp(ntpc->serv_addr, &(sa_in->sin_addr), 4)!=0) 
    {
        return 1;  /* fault */
    }
#else
    (void) ntpc; /* not used */
#endif

    if (NTP_PORT != ntohs(sa_in->sin_port)) 
    {
        return 1;  /* fault */
    }
    return 0;
}

static double ntpdiff( struct ntptime *start, struct ntptime *stop)
{
    int a;
    unsigned int b;
    a = stop->coarse - start->coarse;
    if (stop->fine >= start->fine) 
    {
        b = stop->fine - start->fine;
    } 
    else
    {
        b = start->fine - stop->fine;
        b = ~b;
        a -= 1;
    }

    return a*1.e6 + b * (1.e6/4294967296.0);
}

/* Does more than print, so this name is bogus.
* It also makes time adjustments, both sudden (-s)
* and phase-locking (-l).
* sets *error to the number of microseconds uncertainty in answer
* returns 0 normally, 1 if the message fails sanity checks
*/
static MyTimeval rfc1305print(u32 *data, struct ntptime *arrival, struct ntp_control *ntpc, int *error)
{
    MyTimeval tv_set;
    memset(&tv_set, 0, sizeof(tv_set));

    /* straight out of RFC-1305 Appendix A */
    int li = 0;
    int vn = 0;
    int mode = 0;
    int stratum = 0;
    int poll = 0;
    int prec = 0;
    int delay = 0;
    int disp = 0;
    int refid = 0;
    double el_time = 0.0;
    double st_time = 0.0;
    double skew1 = 0.0;
    double skew2 = 0.0;
    struct ntptime reftime, orgtime, rectime, xmttime;
#ifdef ENABLE_DEBUG
    const char *drop_reason=NULL;
#endif

#define Data(i) ntohl(((u32 *)data)[i])
    li      = Data(0) >> 30 & 0x03;
    vn      = Data(0) >> 27 & 0x07;
    mode    = Data(0) >> 24 & 0x07;
    stratum = Data(0) >> 16 & 0xff;
    poll    = Data(0) >>  8 & 0xff;
    prec    = Data(0)       & 0xff;
    if (prec & 0x80) prec|=0xffffff00;
    delay   = Data(1);
    disp    = Data(2);
    refid   = Data(3);
    reftime.coarse = Data(4);
    reftime.fine   = Data(5);
    orgtime.coarse = Data(6);
    orgtime.fine   = Data(7);
    rectime.coarse = Data(8);
    rectime.fine   = Data(9);
    xmttime.coarse = Data(10);
    xmttime.fine   = Data(11);
#undef Data

    if (debug) 
    {
        printf("LI=%d  VN=%d  Mode=%d  Stratum=%d  Poll=%d  Precision=%d\n",
            li, vn, mode, stratum, poll, prec);
        printf("Delay=%.1f  Dispersion=%.1f  Refid=%u.%u.%u.%u\n",
            sec2u(delay),sec2u(disp),
            refid>>24&0xff, refid>>16&0xff, refid>>8&0xff, refid&0xff);
        printf("Reference %u.%.6u\n", reftime.coarse, USEC(reftime.fine));
        printf("(sent)    %u.%.6u\n", ntpc->time_of_send[0], USEC(ntpc->time_of_send[1]));
        printf("Originate %u.%.6u\n", orgtime.coarse, USEC(orgtime.fine));
        printf("Receive   %u.%.6u\n", rectime.coarse, USEC(rectime.fine));
        printf("Transmit  %u.%.6u\n", xmttime.coarse, USEC(xmttime.fine));
        printf("Our recv  %u.%.6u\n", arrival->coarse, USEC(arrival->fine));
    }

    el_time = ntpdiff(&orgtime, arrival);   /* elapsed */
    st_time = ntpdiff(&rectime, &xmttime);  /* stall */
    skew1 = ntpdiff(&orgtime, &rectime);
    skew2 = ntpdiff(&xmttime, arrival);

    if (debug)
    {
        printf("el_time:%f, st_time:%f, skew1:%f, skew2:%f\n", el_time, st_time, skew1, skew2);
    }

    /* error checking, see RFC-4330 section 5 */
#ifdef ENABLE_DEBUG
#define FAIL(x) do { drop_reason=(x); goto fail;} while (0)
#else
#define FAIL(x) goto fail;
#endif
    if (ntpc->cross_check) 
    {
        if (li == 3) FAIL("LI==3");  /* unsynchronized */
        if (vn < 3) FAIL("VN<3");   /* RFC-4330 documents SNTP v4, but we interoperate with NTP v3 */
        if (mode != 4) FAIL("MODE!=3");
        if (orgtime.coarse != ntpc->time_of_send[0] ||
            orgtime.fine   != ntpc->time_of_send[1] ) FAIL("ORG!=sent");
        if (xmttime.coarse == 0 && xmttime.fine == 0) FAIL("XMT==0");
        if (delay > 65536 || delay < -65536) FAIL("abs(DELAY)>65536");
        if (disp  > 65536 || disp  < -65536) FAIL("abs(DISP)>65536");
        if (stratum == 0) FAIL("STRATUM==0");  /* kiss o' death */
#undef FAIL
    }

    /* it would be even better to subtract half the slop */
    tv_set.tv_sec  = xmttime.coarse - JAN_1970;
    /* divide xmttime.fine by 4294.967296 */
    tv_set.tv_usec = USEC(xmttime.fine);

    return tv_set;
fail:
#ifdef ENABLE_DEBUG
    printf("%d %.5d.%.3d  rejected packet: %s\n",
        arrival->coarse/86400, arrival->coarse%86400,
        arrival->fine/4294967, drop_reason);
#else
    printf("%d %.5d.%.3d  rejected packet\n",
        arrival->coarse/86400, arrival->coarse%86400,
        arrival->fine/4294967);
#endif
    return tv_set;
}

static int stuff_net_addr(struct in_addr *p, char *hostname)
{
    struct hostent *ntpserver;
    ntpserver = gethostbyname(hostname);
    if (ntpserver == NULL) 
    {
        herror(hostname);
        return -1;
    }
    if (ntpserver->h_length != 4) 
    {
        /* IPv4 only, until I get a chance to test IPv6 */
        fprintf(stderr,"oops %d\n",ntpserver->h_length);
        return -1;
    }
    memcpy(&(p->s_addr),ntpserver->h_addr_list[0],4);

    return 0;
}

static int setup_receive(int usd, unsigned int interface, short port)
{
    struct sockaddr_in sa_rcvr;
    memset(&sa_rcvr, 0, sizeof(sa_rcvr));
    sa_rcvr.sin_family = AF_INET;
    sa_rcvr.sin_addr.s_addr = htonl(interface);
    sa_rcvr.sin_port = htons(port);
    if(bind(usd,(struct sockaddr *) &sa_rcvr, sizeof(sa_rcvr)) == -1) 
    {
        perror("bind");
        fprintf(stderr,"could not bind to udp port %d\n",port);
        return -1;
    }
    /* listen(usd,3); this isn't TCP; thanks Alexander! */
    return 0;
}

static int setup_transmit(int usd, char *host, short port, struct ntp_control *ntpc)
{
    struct sockaddr_in sa_dest;
    memset(&sa_dest, 0, sizeof(sa_dest));
    sa_dest.sin_family = AF_INET;
    if(stuff_net_addr(&(sa_dest.sin_addr), host) < 0)
    {
        perror("stuff_net_addr");
        return -1;
    }
        
    memcpy(ntpc->serv_addr,&(sa_dest.sin_addr), 4); /* XXX asumes IPv4 */
    sa_dest.sin_port = htons(port);
    if (connect(usd, (struct sockaddr *)&sa_dest, sizeof(sa_dest)) == -1)
    {
        perror("connect");
        return -1;
    }

    return 0;
}

static SetTimeCallback s_ptrTimeCallBack = NULL;
void ntp_register_time_callback(SetTimeCallback cb)
{
    s_ptrTimeCallBack = cb;
}

void ntp_unregister_time_callback(void)
{
    s_ptrTimeCallBack = NULL;
}

int ntp_thread(void *ctx, int *bStart)
{
    int usd = -1;  /* socket */
    struct ntp_control *pstNtpArgs = (struct ntp_control*)ctx;

    if( NULL == pstNtpArgs)
    {
        goto __exit;
    }

    /* Startup sequence */
    if ((usd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) == -1) 
    {
        perror("socket");
        goto __exit;
    }

    short int udp_local_port = 0;   /* default of 0 means kernel chooses */
    if( setup_receive(usd, INADDR_ANY, udp_local_port) < 0)
    {
        perror("setup_receive");
        goto __exit;
    }

    if(setup_transmit(usd, pstNtpArgs->hostname, NTP_PORT, pstNtpArgs) < 0)
    {
        goto __exit;
    }

    fd_set fds;
    struct sockaddr_in sa_xmit_in;
    int pack_len, probes_sent, error;
    socklen_t sa_xmit_len;
    struct timeval stTimeOut = {0};
    struct ntptime udp_arrival_ntp;
    static u32 incoming_word[325];
	unsigned int tLastOK = 0;
	unsigned int tInterval = 0;
	unsigned int uFromTime = 0;

#define incoming ((char *) incoming_word)
#define sizeof_incoming (sizeof(incoming_word))

    probes_sent = 0;
    sa_xmit_len = sizeof(sa_xmit_in);

    while(bStart && *bStart)
    {
        if (pstNtpArgs->live <= 0)
        {
            break;
        }

        //上次校时成功，则按设置的来，否则每5秒尝试一次
		if(tLastOK > 0)
			tInterval = ntpc.cycle_time;
		else
			tInterval = 5;

        unsigned int uNowTime = GetCurrentTimeStamp();
        if(uNowTime - uFromTime < tInterval * 1000)
        {   
            usleep(100 * 1000);
            continue;
        }

        tLastOK = 0;
        send_packet(usd, ntpc.time_of_send);

        FD_ZERO(&fds);
        FD_SET(usd, &fds);

        int iRet = 0;
        int recv_try_times = 0;
        while(*bStart && ntpc.live > 0 && recv_try_times++ < 10)   //尝试10次，收不到就重新请求
        {
            stTimeOut.tv_sec = 1;
            stTimeOut.tv_usec = 0;
            iRet = select(usd + 1, &fds, NULL, NULL, &stTimeOut);  /* Wait on read or error */
            if(iRet <= 0)
            {
                break;
            }

            pack_len = recvfrom(usd, incoming, sizeof_incoming, 0, (struct sockaddr *) &sa_xmit_in, &sa_xmit_len);
            error = ntpc.goodness;

            if (pack_len > 0 && (unsigned)pack_len<sizeof_incoming)
            {
                get_packet_timestamp(usd, &udp_arrival_ntp);
                if(check_source(pack_len, &sa_xmit_in, sa_xmit_len, &ntpc) != 0)
                    continue;

                MyTimeval tv_set = rfc1305print(incoming_word, &udp_arrival_ntp, &ntpc, &error);
                if(tv_set.tv_sec == 0 && tv_set.tv_usec == 0)
                    continue;
                
                ntpc.set_clock = 1;
                ++probes_sent;

                __INFO("ntp adjust time! %04d(now %u) tv_sec=%u, tv_usec=%u\n", probes_sent, GetCurrentTimeStamp(), tv_set.tv_sec, tv_set.tv_usec);
                if (s_ptrTimeCallBack != NULL)
                {
                    s_ptrTimeCallBack(tv_set.tv_sec, tv_set.tv_usec);
                }

                tLastOK = 1;
                break;
            }
            else
            {
                __ERR("recvlen %d (should < %d) error from NTP server\n", pack_len, sizeof_incoming);
            }

            uFromTime = uNowTime;
            break;
        }

        if (!ntpc.live)
        {
            break;
        }

        /* best rollover option: specify -g, -s, and -l.
        * simpler rollover option: specify -s and -l, which
        * triggers a magic -c 1 */
        if (probes_sent >= ntpc.probe_count && ntpc.probe_count != 0) 
        {
            break;
        }
    }

    __INFO("exit ntp thread loop!\n");

__exit:
    if(usd >= 0)
    {
        close(usd);
        usd = -1;
    }

    pstNtpArgs->live = 0;
    return 0;    
}

int ntp_stop_get_time() 
{
    ntpc.live = 0;
    if (ntpc.ntp_thread.start > 0)
    {
        anj_thread_task_destroy(&ntpc.ntp_thread, 0);
    }

    memset(&ntpc, 0, sizeof(ntpc));
    return 0;
}

int ntp_start_get_time(char* hostname, int cycle_time, int times, int block) 
{
    if (hostname == NULL) 
    {
        return -1;
    }

    if( ntpc.ntp_thread.start > 0 )
    {
        ntp_stop_get_time();
    }

    int iRet = 0;
    ntpc.live = 1;
    ntpc.set_clock = 0;
    ntpc.probe_count = times;           /* default of 0 means loop forever */
    ntpc.cycle_time = cycle_time;          /* seconds */
    ntpc.goodness = 0;
    ntpc.cross_check = 0;//1;
    StrCpy(ntpc.hostname, sizeof(ntpc.hostname), hostname);

    /* respect only applicable MUST of RFC-4330 */
    if (ntpc.probe_count != 1 && ntpc.cycle_time < MIN_INTERVAL) 
    {
        ntpc.cycle_time = MIN_INTERVAL;
    }

    memset(&ntpc.ntp_thread, 0, sizeof(ntpc.ntp_thread));

    ntpc.ntp_thread.bAutoDestroy = 1;
    strncpy(ntpc.ntp_thread.iThreadName, "ntp_thread", sizeof(ntpc.ntp_thread.iThreadName) - 1);
    ntpc.ntp_thread.iThreadjob.ctx = &ntpc;
    ntpc.ntp_thread.iThreadjob.func = ntp_thread;
    iRet = anj_thread_task_create(&ntpc.ntp_thread);
    if (iRet)
    {
        __ERR("ntp thread create failed!\n");
        return -1;
    }

    if(block)
    {
        while(ntpc.live > 0)
        {
            usleep(10 * 1000);
        }
    }

    if(ntpc.set_clock > 0)
        return 0;
    else
        return -1;
}
