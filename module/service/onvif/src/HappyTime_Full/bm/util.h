/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef	__H_UTIL_H__
#define	__H_UTIL_H__


/*************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/

#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#undef MAX
//#include "global.h"
#define MAX(a, b)           ((a) > (b) ? (a) : (b))// 重新定义

//#define MAX(a, b)           ((a) > (b) ? (a) : (b))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(ary)     (sizeof(ary) / sizeof(ary[0]))
#endif

/*************************************************************************/
HT_API int          get_if_nums();
HT_API uint32       get_if_ip(int index);
HT_API uint32       get_route_if_ip(uint32 dst_ip);
HT_API uint32       get_default_if_ip();
HT_API const char * get_local_ip();
HT_API uint32       get_if_mask(int index);
HT_API uint32       get_route_if_mask(uint32 dst_ip);
HT_API uint32       get_default_if_mask();
HT_API int          is_local_if_net(uint32 destip);
HT_API int          get_default_if_mac(uint8 * mac);
HT_API uint32       get_address_by_name(const char * host_name);
HT_API const char * get_default_gateway();
HT_API const char * get_dns_server();
HT_API const char * get_mask_by_prefix_len(int len);
HT_API int          get_prefix_len_by_mask(const char * mask);
HT_API const char * get_ip_str_(uint32 ipaddr /* network byte order */);


/*************************************************************************/
HT_API char       * lowercase(char * str);
HT_API char       * uppercase(char * str);
HT_API int          unicode(char ** dst, char * src);

HT_API BOOL         bin_to_hex_str(uint8 * bin, int binlen, char * hex, int hexlen);
HT_API int          hex_str_to_bin(char * hex, int hexlen, uint8 * bin, int binlen);

HT_API int          url_encode(const char * src, const int srcsize, char * dst, const int dstsize);
HT_API int          url_decode(char * dst, char const * src, uint32 len);
HT_API void         url_split(char const* url, char *proto, int proto_size, char *user, int user_size, char *pass, int pass_size, char *host, int host_size, int *port, char *path, int path_size);

/*************************************************************************/
HT_API time_t       get_time_by_string(char * p_time_str);
HT_API void         get_time_str(char * buff, int len);
HT_API void         get_time_str_day_off(time_t nt, char * buff, int len, int dayoff);
HT_API void         get_time_str_mon_off(time_t nt, char * buff, int len, int moffset);
HT_API time_t       get_time_by_tstring(const char * p_time_str);
HT_API void         get_tstring_by_time(time_t t, char * buff, int len);

HT_API SOCKET       tcp_connect_timeout(uint32 rip, int port, int timeout);

/*************************************************************************/
HT_API void         network_init();
HT_API int          daemon_init();

#if __WINDOWS_OS__
HT_API int          gettimeofday(struct timeval* tp, int* tz);
#endif

#ifdef __cplusplus
}
#endif

#endif	//	__H_UTIL_H__



