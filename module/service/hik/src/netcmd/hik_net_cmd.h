#ifndef __HIK_NET_CMD_H__
#define __HIK_NET_CMD_H__

#include <stddef.h>
#include "hik_net_types.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT32 hik_check_byte_sum(const char *buf, int len);
int hik_writen(int fd, const void *buf, size_t len);
int hik_readn(int fd, void *buf, size_t len);
int hik_send_retval(int fd, UINT32 retVal);
void hik_fill_checksum(void *pkt, size_t total_len);

/*
 * Dispatch one Hik net command.
 * Return: 0 = handled, caller should close fd
 *         1 = handled, caller must keep fd open (ownership transferred)
 *        -1 = error, caller should close fd
 */
int hik_net_dispatch(int fd, UINT32 netCmd, const char *recvbuf, int recvlen);

#ifdef __cplusplus
}
#endif

#endif
