#ifndef __ANJ_MW_ICMP_H__
#define __ANJ_MW_ICMP_H__

#if defined (__cplusplus)
extern "C" {
#endif

#define ICMP_HEADSIZE           8  
#define ICMP_ECHOREPLY          0   /* Echo应答*/    
#define ICMP_ECHO               8   /*Echo请求*/

/**
*    addr_url : 需要ping的字符串, 可传入域名或ip的字符串
*    timeout_ms : 超时时间, 单位ms, 超过该时间未收到回馈数据则按失败处理
*    return : 1 成功, 0 失败
*/
int icmp_ping_url(char *addr_url, int timeout_ms);

#if defined (__cplusplus)
}
#endif


#endif

