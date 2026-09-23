/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef ONVIF_UTIL_H
#define ONVIF_UTIL_H

#include "sys_inc.h"
#include "onvif.h"

#ifdef __cplusplus
extern "C" {
#endif

void onvif_get_time_str(char * buff, int len, int sec_off);
BOOL onvif_is_valid_hostname(const char * name);
BOOL onvif_is_valid_timezone(const char * tz);
void onvif_get_timezone(char * tz, int len);

const char * onvif_uuid_create();
const char * onvif_get_local_ip();

void get_my_macaddr(char macaddr[MACH_ADDR_LENGTH]);
in_addr_t get_netmask_by_prefix_len(unsigned int prefix_len);
in_addr_t get_gateway_by_prefix_len(unsigned int IP, unsigned int prefix_len);

int netsplit( char *pAddress, void *ip );
int ipv4_str_to_num(char *data, struct in_addr *ipaddr);
void OnvifGetVideoSize(char *resName, int tvsystem, int *width, int *height);
RESOLUTION_ENTRY *  GetResolution(char *encode,int width, int height, int tvsystem, int streamno);
int get_rtsp_port(void);

#ifdef __cplusplus
}
#endif


#endif


