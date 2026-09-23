#ifndef _ANJ_MW_PING_H_
#define _ANJ_MW_PING_H_

#if defined (__cplusplus)
extern "C" {
#endif

int try_ping(char *ips, int timeout, int cnt, const char *net_dev, const int *run_flag);

int arpping(unsigned int destIp, unsigned int sourceIp, const char *mac, int timeOutMs, char *ifname);

#if defined (__cplusplus)
}
#endif

#endif
