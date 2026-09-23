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

#ifndef _ONVIF_EXTOOL_H_
#define _ONVIF_EXTOOL_H_

#include "sys_inc.h"
#include "onvif.h"


#define LARGE_INFO_LENGTH 1024
#define AJ_OEM_STR_LEN 32
#define MACH_ADDR_LENGTH 6
#define EXIST 1
#define NOT_EXIST 0
#define ETH_NAME_LOCAL "eth0:0"
#define MID_INFO_LENGTH         40
#define USER_LEN				32 		///< Maximum of acount username length.
#define PASSWORD_LEN			16 		///< Maximum of acount password length.
#define ACOUNT_NUM				16 		///< How many acounts which are stored in system.

//////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////


typedef struct{
	char	user_id[USER_LEN];
	char	password[PASSWORD_LEN];
	unsigned char	authority;
}add_user_t;


// typedef union __NET_IPV4
// {
// 	unsigned long	int32;
// 	char str[4];
// } NET_IPV4;

//////////////////////////////////////////////////////////////////////////////////////////
extern void get_my_ifname(char *ifname);
extern in_addr_t get_my_ipaddr(void);

void get_my_macaddr(char macaddr[MACH_ADDR_LENGTH]);
void onvif_get_sn(char * sn_t);
void onvif_get_uuid(char * uuid);
void OnvifGetVideoSize(char *resName, int tvsystem, int *width, int *height);

int get_web_port(void);
int get_rtsp_port(void);
int get_username_and_password(char *username, char *password);
RESOLUTION_ENTRY *  GetResolution(char *encode,int width, int height, int tvsystem, int streamno);

int oset_timezone(char *TZ,int *pOffsetMin);
int parse_timezone_DaylightSavings(char *TZ,int *pTZ_num,int *pOffsetMin,SummerTimeConfig *pDst);
int oset_getindexbyTZ(int OffsetMin);

int checkhostname(char *hostname);

in_addr_t get_netmask_by_prefix_len(unsigned int prefix_len);
in_addr_t get_gateway_by_prefix_len(unsigned int IP, unsigned int prefix_len);
int netsplit( char *pAddress, void *ip );
int ipv4_str_to_num(char *data, struct in_addr *ipaddr);

int isValidIp4 (char *str);
int isValidHostname (char *str);

int GetNtpServer(char *buf);
int SetNtpConfig(char *ntpServer);

void GetOverlayPos(int SetType, char *onvif_pos_type,float onvif_pos_x,float onvif_pos_y, Positiontype *pos_type, int *pos_x, int *pos_y);
int set_osd(char *token, char * content_format,char *pos_type,float pos_x,float pos_y);

int oset_GetSecFromMonthWeekNo(int year, int month, int weekno, int weekday);

void Aj_Get_ActiveCells(const MotionDetectAlarm *p_md_alarm,      char **Value);

extern int ToBin(int a, char *buf);
extern int ToInt(const char *pbin);

int tiff6_unPackBits(char Pack[], int count, unsigned char array[] /*= NULL*/);

int get_HB_MDCfgByXml(MotionDetectAlarm *mdCfg, char *xmlBuf);

int get_xml_value_Date(char *buf, char *node_start, char *node_end, char *dest);

int Set_date(char *Date_msg, int flag, PdAlarm *pAlarmt);

int get_xml_valueCoordinate(char *buf, char *dest, int flag);

int Get_ElementItem_Coordinate_Value(char *Msg, int **xy_arry, int x_num, int y_num);

int Get_Elementltem_Coordinate_Num(char *Msg, int *flag);

int get_url_http(char *first_value, char *url);

int get_https_port(void);

#ifdef __cplusplus
}
#endif


#endif


