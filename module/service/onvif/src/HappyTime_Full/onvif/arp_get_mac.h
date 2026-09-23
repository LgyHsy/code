#ifndef __ARP_GET_MAC_H__
#define __ARP_GET_MAC_H__

#define ARP_MAC_BYTE 6    
#define ARP_TIME_OUT_MS 200

/**

返回值 :		-1	错误
			0	不存在
			1	存在
			2	请求IP是本设备IP,且IP可用
*/
extern int arp_get_mac(const char *if_name, const char *str_src_ip, const char *str_dst_ip,unsigned char *dst_mac,int timeout_ms); 

#endif
