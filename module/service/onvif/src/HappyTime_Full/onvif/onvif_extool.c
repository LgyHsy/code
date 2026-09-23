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
#include "sys_inc.h"
#include "onvif_extool.h"
#include "anj_mw_comm.h"
#include "onvif.h"
#include "para.h"
#include "anj_mw_net.h"
#include "anj_mw_crypt.h"

extern ONVIF_CFG g_onvif_cfg;
extern WIFIApConfig g_wifiapCfg;
extern int g_ExistWifi;
extern int g_product_type;
extern MediaStreamConfig gStreamCfg;

void get_my_macaddr(char macaddr[MACH_ADDR_LENGTH])
{
	static char __my__mac[MACH_ADDR_LENGTH] = {0,0,0,0,0,0};
	if(memcmp(__my__mac, "\x0\x0\x0\x0\x0\x0", 6) != 0)
	{
		memcpy(macaddr, __my__mac, MACH_ADDR_LENGTH);
		return;
	}

	char ifname[256] = {0};
	get_my_ifname(ifname);
	net_get_hwaddr(ifname, (unsigned char *)macaddr);
	memcpy(__my__mac, macaddr, MACH_ADDR_LENGTH);
}

void onvif_get_uuid(char * uuid)
{
	char g_uuid[64] = {0};
	char macaddr[MACH_ADDR_LENGTH] = {0};
	get_my_macaddr(macaddr);
	sprintf(g_uuid, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);
	sprintf(uuid, "1419d68a-1dd2-11b2-a105-%s", g_uuid);

	return ;

}

void onvif_get_sn(char * sn_t)
{
	unsigned char sn[256];
	if (anj_sysmng_get_sn(sn, sizeof(sn)) >= 0)
		sprintf(sn_t, "%02X%02X%02X%02X%02X%02X%02X%02X", sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);
	return ;
}

void OnvifGetVideoSize(char *resName, int tvsystem, int *width, int *height)
{
	if(tvsystem != 0)
		tvsystem = 1;
	else 
		tvsystem = 0;

	GetVideoSize(resName, tvsystem, width, height);

	//以下为兼容海康NVR无法识别分辨率、不显示的问题
	if( strcmp(resName, "2592X1520") == 0 )
	{
		*width = 2688;
		*height = 1520;
	}
	else if( strcmp(resName, "2592X1512") == 0 )
	{
		*width = 2688;
		*height = 1520;
	}
	else if( strcmp(resName, "2048X1520") == 0 )
	{
		*width = 2048;
		*height = 1536;
	}
	return;
}

int get_web_port(void)
{
	int web_port = gStreamCfg.webConfig.webPort;	
	if(web_port <= 0  || web_port >= 65535)
	{
		log_print(HT_LOG_INFO, "gStreamCfg.webConfig.webPort invalid = %d, try fallback\n",
				web_port);

		short port;
		FILE *web_port_fp = fopen("/tmp/web_port","rb");
		if(web_port_fp)
		{
			fread(&port, 1, sizeof(short), web_port_fp);
			fclose(web_port_fp);

			log_print(HT_LOG_INFO, "read web port from /tmp/web_port OK, port = %d\n", port);
			return port;
		}
		else
		{
			log_print(HT_LOG_INFO, "read web port from /tmp/web_port failed, use default port = %d\n", 80);
			return 80;
		}
	}
	else
		return web_port;
}

int get_rtsp_port(void)
{
	int video_port = g_onvif_cfg.network.NetworkProtocol.RTSPPort[0];
	if(video_port <= 0  || video_port >= 65535)
	{
		log_print(HT_LOG_INFO, "gStreamCfg.rtspConfig.videoPort invalid = %d, try fallback\n",
				video_port);

		short port;
		FILE *video_port_fp = fopen("/tmp/rtsp_port","rb");
		if(video_port_fp)
		{
			fread(&port, 1, sizeof(short), video_port_fp);
			fclose(video_port_fp);

			log_print(HT_LOG_INFO, "read video port from /tmp/rtsp_port OK, port = %d\n", port);
			return port;
		}
		else
		{
			log_print(HT_LOG_INFO, "read video port from /tmp/rtsp_port failed, use default port = %d\n", 554);
			return 554;
		}
	}
	else
		return video_port;
}

int get_username_and_password(char *username, char *password)
{
	BOOL bNoAuth = TRUE;
	if( bNoAuth) 
	{
		MediaStreamConfig *pTmpStreamCfg;
		UserConfig usrCfg;
		int j;
		pTmpStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
		if(pTmpStreamCfg)
		{			
			if(pTmpStreamCfg->rtspConfig.rtsp_auth)
			{
				SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
				if (!pSystemCfg)
				{
					return -1;
				}
				memcpy(&usrCfg, &pSystemCfg->userCfg, sizeof(usrCfg));
				for(j = 0; j < usrCfg.count; j++)
				{
					if(!strcmp(usrCfg.accounts[j].group.groupName, "Administrator") && !strcmp(usrCfg.accounts[j].status, "Enable"))
					{
						strcpy(username, usrCfg.accounts[j].userName);
						our_md5_encode(password, (const unsigned char *)usrCfg.accounts[j].password, strlen(usrCfg.accounts[j].password));
						break;
					}
				}
			}
		}
	}
	return 0;
}

RESOLUTION_ENTRY *  GetResolution(char *encode,int width, int height, int tvsystem, int streamno)
{
	RESOLUTION_ENTRY *pEntry = NULL;
	RESOLUTION_ENTRY *pEntryArray = NULL;
	int nrescount = anj_sysmng_video_res_array_get(&pEntryArray);
	if( nrescount == 0 || pEntryArray == NULL )
	{
		return pEntry;
	}


	int iIndex = 0;
	for( iIndex = 0; iIndex < nrescount; iIndex++)
	{
		int w, h;

		OnvifGetVideoSize(pEntryArray[iIndex].res_name, tvsystem, &w, &h);
		if(!strcmp(encode,pEntryArray[iIndex].codec_name)){
			if( width == w && height == h && pEntryArray[iIndex].stream_type == streamno )
			{
				pEntry = &pEntryArray[iIndex];
				return pEntry;
			}
		}
	}

	return NULL;
}

int oset_getindexbyTZ(int OffsetMin)
{
	if(OffsetMin == -12*60) 	return 0;
	if(OffsetMin == -11*60) 	return 1;
	if(OffsetMin == -10*60) 	return 2;
	if(OffsetMin == -9*60)		return 3;
	if(OffsetMin == -8*60)		return 4;
	if(OffsetMin == -7*60)		return 5;
	if(OffsetMin == -6*60)		return 6;
	if(OffsetMin == -5*60)		return 7;
	if(OffsetMin == -4*60)		return 8;
	if(OffsetMin == -3*60)		return 9;
	if(OffsetMin == -2*60)		return 10;
	if(OffsetMin == -1*60)		return 11;
	if(OffsetMin == 0)			return 12;
	if(OffsetMin == 0*60)		return 12;
	if(OffsetMin == 1*60)		return 13;
	if(OffsetMin == 2*60)		return 14;
	if(OffsetMin == 3*60)		return 15;
	if(OffsetMin == 4*60)		return 16;
	if(OffsetMin == 5*60)		return 17;
	if(OffsetMin == 6*60)		return 18;
	if(OffsetMin == 7*60)		return 19;
	if(OffsetMin == 8*60)		return 20;
	if(OffsetMin == 9*60)		return 21;
	if(OffsetMin == 10*60)		return 22;
	if(OffsetMin == 11*60)		return 23;
	if(OffsetMin == 12*60)		return 24;
	if(OffsetMin == 3*60+30)	return 25;
	if(OffsetMin == 4*60+30)	return 26;
	if(OffsetMin == 5*60+30)	return 27;
	if(OffsetMin == 6*60+30)	return 28;
	if(OffsetMin == -4*60-30)	return 29;
	if(OffsetMin == -3*60-30)	return 30;
	if(OffsetMin == 9*60+30)	return 31;
	if(OffsetMin == 5*60+45)	return 32;
	if(OffsetMin == 13*60)		return 33;
	if(OffsetMin == 11*60+30)	return 34;
	if(OffsetMin == 12*60+45)	return 35;

	return 100;
}

int parse_timezone_DaylightSavings(char *TZ,int *pTZ_num,int *pOffsetMin,SummerTimeConfig *pDst)
{
	int tz_num = -1;
	int offset_min = 0;

	int bGetDLInfo = 0;
	
	if(strlen(TZ)<=0){
		*pTZ_num = tz_num;
		*pOffsetMin = offset_min;
		return -1;
	}

#if 0//csj 20190319
	if(!strncmp(TZ,"<GMT+12>+12",strlen("<GMT+12>+12")))	tz_num=0,offset_min=-12*60; //-12
	else if(!strncmp(TZ,"GMT+12",strlen("GMT+12")))			tz_num=1,offset_min=-12*60; //-12
	else if(!strncmp(TZ,"<GMT+11>+11",strlen("<GMT+11>+11")))	tz_num=1,offset_min=-11*60; //-11
	else if(!strncmp(TZ,"GMT+11",strlen("GMT+11")))			tz_num=1,offset_min=-11*60; //-11
	else if(!strncmp(TZ,"HAST10HADT",strlen("HAST10HADT")))	tz_num=2,offset_min=-10*60; //-10
	else if(!strncmp(TZ,"AKST9AKDT",strlen("AKST9AKDT")))	tz_num=3,offset_min=-9*60; //-9
	else if(!strncmp(TZ,"PST8PDT",strlen("PST8PDT")))			tz_num=4,offset_min=-8*60; //-8
	else if(!strncmp(TZ,"MST7MDT",strlen("MST7MDT")))		tz_num=5,offset_min=-7*60; //-7
	else if(!strncmp(TZ,"PNT7",strlen("PNT7")))				tz_num=5,offset_min=-7*60; //-7
	else if(!strncmp(TZ,"CST6CDT",strlen("CST6CDT")))		tz_num=6,offset_min=-6*60; //-6
	else if(!strncmp(TZ,"CST5CDT",strlen("CST5CDT")))		tz_num=7,offset_min=-5*60; //-5
	else if(!strncmp(TZ,"EST5EDT",strlen("EST5EDT")))		tz_num=7,offset_min=-5*60; //-5
	else if(!strncmp(TZ,"VET4:30",strlen("VET4:30")))		tz_num=29,offset_min=-4*60-30; //-4:30
	else if(!strncmp(TZ,"AST4:30",strlen("AST4:30")))		tz_num=29,offset_min=-4*60-30; // -4:30	
	else if(!strncmp(TZ,"CST4CDT",strlen("CST4CDT")))		tz_num=8,offset_min=-4*60; //-4
	else if(!strncmp(TZ,"AMT4AMST",strlen("AMT4AMST")))		tz_num=8,offset_min=-4*60; //-4
	else if(!strncmp(TZ,"AST4",strlen("AST4")))				tz_num=8,offset_min=-4*60; //-4
	else if(!strncmp(TZ,"PYT4PYST",strlen("PYT4PYST")))		tz_num=8,offset_min=-4*60; //-4
	else if(!strncmp(TZ,"CLT4CLST",strlen("CLT4CLST")))		tz_num=8,offset_min=-4*60; //-4
	else if(!strncmp(TZ,"GMT+4",strlen("GMT+4")))			tz_num=8,offset_min=-4*60; //-4	
	else if(!strncmp(TZ,"CNT3:30",strlen("CNT3:30")))		tz_num=30,offset_min=-3*60-30; //-3:30
	else if(!strncmp(TZ,"NST3:30NDT",strlen("NST3:30NDT")))	tz_num=30,offset_min=-3*60-30; //-3:30
	else if(!strncmp(TZ,"BRT3:30",strlen("BRT3:30")))		tz_num=30,offset_min=-3*60-30; //-3:30
	else if(!strncmp(TZ,"BRT3BRST",strlen("BRT3BRST")))		tz_num=9,offset_min=-3*60; //-3
	else if(!strncmp(TZ,"ART3",strlen("ART3")))				tz_num=9,offset_min=-3*60; //-3
	else if(!strncmp(TZ,"WGT3WGST",strlen("WGT3WGST")))	tz_num=9,offset_min=-3*60; //-3
	else if(!strncmp(TZ,"UYT3UYST",strlen("UYT3UYST")))		tz_num=9,offset_min=-3*60; //-3
	else if(!strncmp(TZ,"FNT2",strlen("FNT2")))				tz_num=10,offset_min=-2*60; //-2
	else if(!strncmp(TZ,"AZOT1AZOST",strlen("AZOT1AZOST")))	tz_num=11,offset_min=-1*60; //-1
	else if(!strncmp(TZ,"AZOT1",strlen("AZOT1")))			tz_num=11,offset_min=-1*60; //-1	
	else if(!strncmp(TZ,"GMT0BST",strlen("GMT0BST")))		tz_num=12,offset_min=0; //0
	else if(!strncmp(TZ,"GMT0",strlen("GMT0")))				tz_num=12,offset_min=0; //0	
	else if(!strncmp(TZ,"CET-1CEST",strlen("CET-1CEST")))		tz_num=13,offset_min=1*60; //+1
	else if(!strncmp(TZ,"WAT1WAST",strlen("WAT1WAST")))		tz_num=13,offset_min=1*60; //+1
	else if(!strncmp(TZ,"WAT-1WAST",strlen("WAT-1WAST")))	tz_num=13,offset_min=1*60; //+1
	else if(!strncmp(TZ,"EET-2EEST",strlen("EET-2EEST")))		tz_num=14,offset_min=2*60; //+2
	else if(!strncmp(TZ,"EET-2",strlen("EET-2")))				tz_num=14,offset_min=2*60; //+2
	else if(!strncmp(TZ,"IST-2IDT",strlen("IST-2IDT")))		tz_num=14,offset_min=2*60; //+2
	else if(!strncmp(TZ,"SAST-2",strlen("SAST-2")))			tz_num=14,offset_min=2*60; //+2
	else if(!strncmp(TZ,"IRT-3:30",strlen("IRT-3:30")))			tz_num=25,offset_min=3*60+30; //+3:30
	else if(!strncmp(TZ,"IRST-3:30IRDT-4:30",strlen("IRST-3:30IRDT-4:30")))	tz_num=25,offset_min=3*60+30; //+3:30
	else if(!strncmp(TZ,"AST-3:30",strlen("AST-3:30")))		tz_num=25,offset_min=3*60+30; //+3:30
	else if(!strncmp(TZ,"AST-3",strlen("AST-3")))			tz_num=15,offset_min=3*60; //+3
	else if(!strncmp(TZ,"MSK-3",strlen("MSK-3")))			tz_num=15,offset_min=3*60; //+3
	else if(!strncmp(TZ,"AFT-4:30",strlen("AFT-4:30")))		tz_num=26,offset_min=4*60+30; //+4:30
	else if(!strncmp(TZ,"AZT-4:30",strlen("AZT-4:30")))		tz_num=26,offset_min=4*60+30; //+4:30
	else if(!strncmp(TZ,"AZT-4AZST",strlen("AZT-4AZST")))	tz_num=16,offset_min=4*60; //+4
	else if(!strncmp(TZ,"GST4",strlen("GST4")))				tz_num=16,offset_min=4*60; //+4
	else if(!strncmp(TZ,"GST-4",strlen("GST-4")))			tz_num=16,offset_min=4*60; //+4
	else if(!strncmp(TZ,"MSK-4",strlen("MSK-4")))			tz_num=16,offset_min=4*60; //+4
	else if(!strncmp(TZ,"AZT-4",strlen("AZT-4")))			tz_num=16,offset_min=4*60; //+4
	else if(!strncmp(TZ,"GMT-5:30",strlen("GMT-5:30")))		tz_num=27,offset_min=5*60+30; //+5:30
	else if(!strncmp(TZ,"IST-5:30",strlen("IST-5:30")))		tz_num=27,offset_min=5*60+30; //+5:30
	else if(!strncmp(TZ,"PKT-5:30",strlen("PKT-5:30")))		tz_num=27,offset_min=5*60+30; //+5:30
	else if(!strncmp(TZ,"NPT-5:45",strlen("NPT-5:45")))		tz_num=32,offset_min=5*60+45; //+5:45
	else if(!strncmp(TZ,"GMT-5:45",strlen("GMT-5:45")))		tz_num=32,offset_min=5*60+45; //+5:45
	else if(!strncmp(TZ,"PKT-5:45",strlen("PKT-5:45")))		tz_num=32,offset_min=5*60+45; //+5:45
	else if(!strncmp(TZ,"PKT-5",strlen("PKT-5")))			tz_num=17,offset_min=5*60; //+5
	else if(!strncmp(TZ,"MMT-6:30",strlen("MMT-6:30")))		tz_num=28,offset_min=6*60+30; //+6:30
	else if(!strncmp(TZ,"NOVT-6:30",strlen("NOVT-6:30")))	tz_num=28,offset_min=6*60+30; //+6:30
	else if(!strncmp(TZ,"ALMT-6",strlen("ALMT-6")))			tz_num=18,offset_min=6*60; //+6
	else if(!strncmp(TZ,"NOVT-6",strlen("NOVT-6")))			tz_num=18,offset_min=6*60; //+6	
	else if(!strncmp(TZ,"WIT-7",strlen("WIT-7")))			tz_num=19,offset_min=7*60; //+7
	else if(!strncmp(TZ,"CST-8",strlen("CST-8")))			tz_num=20,offset_min=8*60; //+8
	else if(!strncmp(TZ,"WST-8",strlen("WST-8")))			tz_num=20,offset_min=8*60; //+8
	else if(!strncmp(TZ,"CST-9:30CST",strlen("CST-9:30CST")))	tz_num=31,offset_min=9*60+30; //+9:30
	else if(!strncmp(TZ,"CST-9:30",strlen("CST-9:30")))		tz_num=31,offset_min=9*60+30; //+9:30
	else if(!strncmp(TZ,"GMT-9:30",strlen("GMT-9:30")))		tz_num=31,offset_min=9*60+30; //+9:30
	else if(!strncmp(TZ,"JST-9:30",strlen("JST-9:30")))			tz_num=31,offset_min=9*60+30; //+9:30
	else if(!strncmp(TZ,"JST-9",strlen("JST-9")))				tz_num=21,offset_min=9*60; //+9
	else if(!strncmp(TZ,"YAKT-10",strlen("YAKT-10")))			tz_num=22,offset_min=10*60; //+10
	else if(!strncmp(TZ,"EST-10EST",strlen("EST-10EST")))		tz_num=22,offset_min=10*60; //+10
	else if(!strncmp(TZ,"NFT-11:30",strlen("NFT-11:30")))		tz_num=23,offset_min=11*60+30; //+11:30
	else if(!strncmp(TZ,"SBT-11",strlen("SBT-11")))			tz_num=23,offset_min=11*60; //+11
	else if(!strncmp(TZ,"GMT-11",strlen("GMT-11")))			tz_num=23,offset_min=11*60; //+11
	else if(!strncmp(TZ,"CHAST-12:45CHADT",strlen("CHAST-12:45CHADT")))	tz_num=33,offset_min=12*60+45; //+12:45
	else if(!strncmp(TZ,"NZST-12NZDT",strlen("NZST-12NZDT")))tz_num=24,offset_min=12*60; //+12
	else if(!strncmp(TZ,"FJT-12",strlen("FJT-12")))			tz_num=24,offset_min=12*60; //+12
	else if(!strncmp(TZ,"PETT-12PETST",strlen("PETT-12PETST")))tz_num=24,offset_min=12*60; //+12
	else if(!strncmp(TZ,"MHT-12",strlen("MHT-12")))			tz_num=24,offset_min=12*60; //+12
	else if(!strncmp(TZ,"SST-13SDT",strlen("SST-13SDT")))	tz_num=33,offset_min=13*60; //+13
	else if(!strncmp(TZ,"SST-13",strlen("SST-13")))			tz_num=33,offset_min=13*60; //+13	
	else if(!strncmp(TZ,"TOT-13",strlen("TOT-13")))			tz_num=33,offset_min=13*60; //+13
	else if(!strncmp(TZ,"GMT-13",strlen("GMT-13")))			tz_num=33,offset_min=13*60; //+13

	*pTZ_num = tz_num;
	*pOffsetMin = offset_min;
#endif

	/*判断夏令时*/
	char strDst[100]={0};
	strcpy(strDst,TZ);
	strDst[99]=0;
	char *m1 = strstr(strDst, ",M");
	char *m2 = NULL;
	if( NULL != m1 ){
		m1 += 1;
		m2 = strstr(m1, ",M");
		if( NULL != m2 ){
			*m2 = 0;
			m2 += 1;
		}
	}

	do{
		if( m1 != NULL && m2!= NULL ){
			int month = 0, weekno = 0, weekday=0, hour = 0;
			
			if(4 != sscanf(m1, "M%d.%d.%d/%d", &month, &weekno, &weekday, &hour))
			{
				if(3 != sscanf(m1, "M%d.%d.%d", &month, &weekno, &weekday))
					return -1;
			}

			pDst->nStartMonth = month;
			pDst->nStartWeek = weekno;
			pDst->nStartWeekday = weekday;
			pDst->nStartHour = hour;

			hour = 0;
			if(4 != sscanf(m2, "M%d.%d.%d/%d", &month, &weekno, &weekday, &hour))
			{
				if(3 != sscanf(m2, "M%d.%d.%d", &month, &weekno, &weekday))
					return -1;
			}

			pDst->nToMonth = month;
			pDst->nToWeek = weekno;
			pDst->nToWeekday = weekday;
			pDst->nToHour = hour;

			pDst->nOffsetMin = 60;

			bGetDLInfo = 1;
		}
#if 1
		/* 
		   夏令时的时间偏移默认为1小时 
		   若字串中有对应字段则进行设置
		   当前时区的夏令时
		 */
		int  index = 0;
		char strNum[16];
		char *str = TZ;

		/* SST11 */
		/* SST11SST */
		/* SST11SST,M3.1.1/2,M11.5.6/16 */
		/* SST11SST10:30,M3.1.1/2,M11.5.6/16 */

		if(*str != '<')
		{
			while((*str >= 'A' && *str <= 'Z')||(*str >= 'a' && *str <= 'z'))
				str += 1;
		}
		else
		{
			while(*str != 0)
			{
				if(*str == '>')
				{
					str += 1;
					break;
				}
				
				str += 1;
			}
		}
		
		if(*str == 0 || !((*str >= '0' && *str <= '9')|| (*str == '-') || (*str == '+') || (*str == ':')))
			break;			

		index = 0;
		while((*str >= '0' && *str <= '9')|| (*str == '-') || (*str == '+') || (*str == ':'))
		{
			strNum[index] = *str;
			index++;
			str += 1;
		}
		strNum[index] = 0;
		if((*str == 0 || !((*str >= 'A' && *str <= 'Z')||(*str >= 'a' && *str <= 'z'))) && tz_num != -1)
		{
			break;
		}

		while((*str >= 'A' && *str <= 'Z')||(*str >= 'a' && *str <= 'z'))
			str += 1;
		
		/* 解析时区字符串 */
		if(tz_num == -1)
		{
			int TZHour;
			int TZMin;
			
			if(strstr(strNum, ":"))
			{
				if(2 != sscanf(strNum, "%d:%d", &TZHour, &TZMin))
					break;				
			
				if(TZHour < 0)
					offset_min = -TZHour*60+TZMin;
				else 
					offset_min = -TZHour*60-TZMin;
			
				//log_print(HT_LOG_ERR, "TZ offset %d TZhour:%d TZmin:%d\n", offset_min, TZHour, TZMin);
			}
			else
			{
				if(1 != sscanf(strNum, "%d", &TZHour))
					break;
			
				offset_min = -TZHour*60;
			
				//log_print(HT_LOG_ERR, "TZ offset %d TZhour:%d \n", offset_min, TZHour);
			}
			
			*pOffsetMin = offset_min;
			*pTZ_num = tz_num = oset_getindexbyTZ(offset_min);
		}
		
		if(*str == 0 || !((*str >= '0' && *str <= '9') || (*str == '-') || (*str == ':')))
		{
			break;
		}

		index = 0;
		while(*str != 0 && *str != ',')
		{
			strNum[index] = *str;
		
			index++;
			str += 1;
		}
		strNum[index] = 0;

		//log_print(HT_LOG_ERR, "strNum %s\n", strNum);

		if(index == 0)//没有偏移字段,默认1小时
			pDst->nOffsetMin = 60;
		else{
			/* 转换 */
			int DLHour;
			int DLMin;
			int DLOffset;

			if(strstr(strNum, ":"))
			{
				if(2 != sscanf(strNum, "%d:%d", &DLHour, &DLMin))
					break;				

				if(DLHour < 0)
					DLOffset = -DLHour*60+DLMin;
				else 
					DLOffset = -DLHour*60-DLMin;

				if(tz_num >= 0)
					pDst->nOffsetMin = DLOffset - offset_min;
				else
					pDst->nOffsetMin = 60;

				//log_print(HT_LOG_ERR, "DL offset %d DLhour:%d DLmin:%d\n", pDst->nOffsetMin, DLHour, DLMin);
			}
			else
			{
				if(1 != sscanf(strNum, "%d", &DLHour))
					break;

				DLOffset = -DLHour*60;
				
				if(tz_num >= 0)
					pDst->nOffsetMin = DLOffset - offset_min;
				else
					pDst->nOffsetMin = 60;			

				//log_print(HT_LOG_ERR, "DL offset %d DLhour:%d\n", pDst->nOffsetMin, DLHour);
			}
		}
#endif
		pDst->bAuto = 0;
		
		return 1;
	}while(0);

	if(bGetDLInfo != 0)
	{	
		return 1;
	}
	
	if(tz_num >= 0)
		return 0;

	return -1;
}

int oset_timezone(char *TZ,int *pOffsetMin)
{
	char *p = NULL;
	*pOffsetMin = 0;

	if(strlen(TZ)<=0)
		return 100;

#if 0//csj 20190319
	/* special handle */
	if(strstr(TZ,"GMT-8") || strstr(TZ,"CST-8") || strstr(TZ,"ChinaStandardTime-8") || strstr(TZ,"TaipeiStandardTime-8") || strstr(TZ,"SingaporeStandardTime-8"))//J 20140106
	{*pOffsetMin = 8*60; return 20;}

	/* 兼容雄迈NVR时区设置 */	
	if(strcmp(TZ,"IDLW12")==0)		{*pOffsetMin = -12*60;return 0;}// -12
	if(strcmp(TZ,"NT11")==0)		{*pOffsetMin = -11*60;return 1;}// -11
	if(strcmp(TZ,"AHST10")==0)		{*pOffsetMin = -10*60;return 2;}// -10
	if(strcmp(TZ,"AKST9AKDT")==0)	{*pOffsetMin = -9*60;return 3;}// -9
	if(strcmp(TZ,"PST8PDT")==0)		{*pOffsetMin = -8*60;return 4;}// -8
	if(strcmp(TZ,"MST7MDT")==0)		{*pOffsetMin = -7*60;return 5;}// -7
	if(strcmp(TZ,"CST6CDT")==0)		{*pOffsetMin = -6*60;return 6;}// -6
	if(strcmp(TZ,"ACT5ACST")==0)	{*pOffsetMin = -5*60;return 7;}// -5
	if(strcmp(TZ,"AST4ADT")==0)		{*pOffsetMin = -4*60;return 8;}// -4
	if(strcmp(TZ,"BRT3")==0)		{*pOffsetMin = -3*60;return 9;}// -3
	if(strcmp(TZ,"FNT2")==0)		{*pOffsetMin = -2*60;return 10;}// -2
	if(strcmp(TZ,"WAT1")==0)		{*pOffsetMin = -1*60;return 11;}// -1
	if(strcmp(TZ,"UTC0")==0)		{*pOffsetMin = 0;return 12;}// 0
	if(strcmp(TZ,"CET-1CEST")==0)	{*pOffsetMin = 1*60;return 13;}// +1
	if(strcmp(TZ,"EET-2EETDST")==0)	{*pOffsetMin = 2*60;return 14;}// +2
	if(strcmp(TZ,"EAT-3")==0)		{*pOffsetMin = 3*60;return 15;}// +3
	if(strcmp(TZ,"EAST-4")==0)		{*pOffsetMin = 4*60;return 16;}// +4
	if(strcmp(TZ,"IOT-5")==0)		{*pOffsetMin = 5*60;return 17;}// +5
	if(strcmp(TZ,"ALMT-6")==0)		{*pOffsetMin = 6*60;return 18;}// +6
	if(strcmp(TZ,"CXT-7")==0)		{*pOffsetMin = 7*60;return 19;}// +7
	if(strcmp(TZ,"AWST-8WDT")==0)	{*pOffsetMin = 8*60;return 20;}// +8
	if(strcmp(TZ,"JST-9")==0)		{*pOffsetMin = 9*60;return 21;}// +9
	if(strcmp(TZ,"AEST-10AESST")==0)	{*pOffsetMin = 10*60;return 22;}// +10
	if(strcmp(TZ,"AESST-11")==0)	{*pOffsetMin = 11*60;return 23;}// +11
	if(strcmp(TZ,"NZT-12")==0)		{*pOffsetMin = 12*60;return 24;}// +12
	if(strcmp(TZ,"IRT,IT-03:30")==0)	{*pOffsetMin = 3*60+30;return 25;}// +3:30
	if(strcmp(TZ,"AFT-4:30")==0)		{*pOffsetMin = 4*60+30;return 26;}// +4:30
	if(strcmp(TZ,"IndiaStandardTime-5:30")==0)	{*pOffsetMin = 5*60+30;return 27;}// +5:30
	if(strcmp(TZ,"MMT-6:30")==0)	{*pOffsetMin = 6*60+30;return 28;}// +6:30
	if(strcmp(TZ,"NST3:30")==0)		{*pOffsetMin = -3*60-30;return 30;}// -3:30
	if(strcmp(TZ,"SAT-9:30SADT")==0){*pOffsetMin = 9*60+30;	return 31;}// +9:30
	if(strcmp(TZ,"NepalStandardTime-5:45")==0)	{*pOffsetMin = 5*60+45;return 32;}// +5:45
	if(strcmp(TZ,"NZDT-13")==0)		{*pOffsetMin = 13*60;return 33;}// +13

	for(i=0;i<TZ_COUNT_V2;i++)
	{
		if(!strcmp(TZ, _TZ_name_hk[i])){
			if(i>=0 && i<TZ_COUNT_V1) *pOffsetMin = (i-12)*60;
			else if(i==25) *pOffsetMin = 3*60+30;
			else if(i==26) *pOffsetMin = 4*60+30;
			else if(i==27) *pOffsetMin = 5*60+30;
			else if(i==28) *pOffsetMin = 6*60+30;
			else if(i==29) *pOffsetMin = -4*60-30;
			else if(i==30) *pOffsetMin = -3*60-30;
			else if(i==31) *pOffsetMin = 9*60+30;
			else if(i==32) *pOffsetMin = 5*60+45;
			else if(i==33) *pOffsetMin = 13*60;	
			return i;
		}
	}

	for(i=0;i<TZ_COUNT_V2;i++)
	{
		if(!strcmp(TZ, _TZ_name_ott[i])){
			if(i>=0 && i<TZ_COUNT_V1) *pOffsetMin = (i-12)*60;
			else if(i==25) *pOffsetMin = 3*60+30;
			else if(i==26) *pOffsetMin = 4*60+30;
			else if(i==27) *pOffsetMin = 5*60+30;
			else if(i==28) *pOffsetMin = 6*60+30;
			else if(i==29) *pOffsetMin = -4*60-30;
			else if(i==30) *pOffsetMin = -3*60-30;
			else if(i==31) *pOffsetMin = 9*60+30;
			else if(i==32) *pOffsetMin = 5*60+45;
			else if(i==33) *pOffsetMin = 13*60;	
			return i;
		}
	}
#endif

	/* normal */
	if(NULL != (p = strchr(TZ, ':')))
	{
		p -= 3;
		if (strncmp(p, "+12:00", 6)==0){*pOffsetMin = -12*60;return 0;}
		else if (strncmp(p, "+11:00", 6)==0){*pOffsetMin = -11*60;return 1;}
		else if (strncmp(p, "+10:00", 6)==0){*pOffsetMin = -10*60;return 2;}
		else if (strncmp(p, "+09:00", 6)==0){*pOffsetMin = -9*60;return 3;}
		else if (strncmp(p, "+08:00", 6)==0){*pOffsetMin = -8*60;return 4;}
		else if (strncmp(p, "+07:00", 6)==0){*pOffsetMin = -7*60;return 5;}
		else if (strncmp(p, "+06:00", 6)==0){*pOffsetMin = -6*60;return 6;}
		else if (strncmp(p, "+05:00", 6)==0){*pOffsetMin = -5*60;return 7;}
		else if (strncmp(p, "+04:00", 6)==0){*pOffsetMin = -4*60;return 8;}
		else if (strncmp(p, "+03:00", 6)==0){*pOffsetMin = -3*60;return 9;}
		else if (strncmp(p, "+02:00", 6)==0){*pOffsetMin = -2*60;return 10;}
		else if (strncmp(p, "+01:00", 6)==0){*pOffsetMin = -1*60;return 11;}
		else if (strncmp(p, "+00:00", 6)==0){*pOffsetMin = 0;return 12;}
		else if (strncmp(p, "-00:00", 6)==0){*pOffsetMin = 0*60;return 12;}
		else if (strncmp(p, "-01:00", 6)==0){*pOffsetMin = 1*60;return 13;}
		else if (strncmp(p, "-02:00", 6)==0){*pOffsetMin = 2*60;return 14;}
		else if (strncmp(p, "-03:00", 6)==0){*pOffsetMin = 3*60;return 15;}
		else if (strncmp(p, "-04:00", 6)==0){*pOffsetMin = 4*60;return 16;}
		else if (strncmp(p, "-05:00", 6)==0){*pOffsetMin = 5*60;return 17;}
		else if (strncmp(p, "-06:00", 6)==0){*pOffsetMin = 6*60;return 18;}
		else if (strncmp(p, "-07:00", 6)==0){*pOffsetMin = 7*60;return 19;}
		else if (strncmp(p, "-08:00", 6)==0){*pOffsetMin = 8*60;return 20;}
		else if (strncmp(p, "-09:00", 6)==0){*pOffsetMin = 9*60;return 21;}
		else if (strncmp(p, "-10:00", 6)==0){*pOffsetMin = 10*60;return 22;}
		else if (strncmp(p, "-11:00", 6)==0){*pOffsetMin = 11*60;return 23;}
		else if (strncmp(p, "-12:00", 6)==0){*pOffsetMin = 12*60;return 24;}
		else if (strncmp(p, "-03:30", 6)==0){*pOffsetMin = 3*60+30;return 25;}
		else if (strncmp(p, "-04:30", 6)==0){*pOffsetMin = 4*60+30;return 26;}
		else if (strncmp(p, "-05:30", 6)==0){*pOffsetMin = 5*60+30;return 27;}
		else if (strncmp(p, "-06:30", 6)==0){*pOffsetMin = 6*60+30;return 28;}
		else if (strncmp(p, "+04:30", 6)==0){*pOffsetMin = -4*60-30;return 29;}
		else if (strncmp(p, "+03:30", 6)==0){*pOffsetMin = -3*60-30;return 30;}
		else if (strncmp(p, "-09:30", 6)==0){*pOffsetMin = 9*60+30;return 31;}
		else if (strncmp(p, "-05:45", 6)==0){*pOffsetMin = 5*60+45;return 32;}
		else if (strncmp(p, "-13:00", 6)==0){*pOffsetMin = 13*60;return 33;}
		else if (strncmp(p, "-11:30", 6)==0){*pOffsetMin = 11*60+30;return 34;}
		else if (strncmp(p, "-12:45", 6)==0){*pOffsetMin = 12*60+45;return 35;}		
	}

	/* special handle for CST-6 from Hikvision NVR, XXX 20130307 */
	if (strncmp(TZ, "IDLW", 4)==0){*pOffsetMin = -12*60;return 0;}//-12
	else if (strncmp(TZ, "NT", 2)==0){*pOffsetMin = -11*60;return 1;}//-11
	else if ((strncmp(TZ, "AHST", 4)==0)||(strncmp(TZ, "CAT", 3)==0)||(strncmp(TZ, "HST", 3)==0)||(strncmp(TZ, "HDT", 3)==0)){*pOffsetMin = -10*60;return 2;}//-10
	else if ((strncmp(TZ, "YST", 3)==0)||(strncmp(TZ, "YDT", 3)==0)){*pOffsetMin = -9*60;return 3;}//-9
	else if ((strncmp(TZ, "PST", 3)==0)||(strncmp(TZ, "PDT", 3)==0)){*pOffsetMin = -8*60;return 4;}//-8
	else if ((strncmp(TZ, "MST", 3)==0)||(strncmp(TZ, "MDT", 3)==0)){*pOffsetMin = -7*60;return 5;}//-7
	else if ((strncmp(TZ, "CST", 3)==0)||(strncmp(TZ, "CDT", 3)==0)){*pOffsetMin = -6*60;return 6;}//-6
	else if ((strncmp(TZ, "EST", 3)==0)||(strncmp(TZ, "EDT", 3)==0)){*pOffsetMin = -5*60;return 7;}//-5
	else if ((strncmp(TZ, "AST", 3)==0)||(strncmp(TZ, "ADT", 3)==0)){*pOffsetMin = -4*60;return 8;}//-4
	else if (strncmp(TZ, "GMT-03", 6)==0){*pOffsetMin = -3*60;return 9;}//-3
	else if (strncmp(TZ, "AT", 2)==0){*pOffsetMin = -2*60;return 10;}//-2
	else if (strncmp(TZ, "WAT", 3)==0){*pOffsetMin = -1*60;return 11;}//-1
	else if ((strncmp(TZ, "GMT", 3)==0)||(strncmp(TZ, "UT", 2)==0)||(strncmp(TZ, "UTC", 3)==0)||(strncmp(TZ, "BST", 3)==0)){*pOffsetMin = 0;return 12;}//-0
	else if ((strncmp(TZ, "CET", 3)==0)||(strncmp(TZ, "FWT", 3)==0)||(strncmp(TZ, "MET", 3)==0)\
			||(strncmp(TZ, "MEWT", 4)==0)||(strncmp(TZ, "SWT", 3)==0)||(strncmp(TZ, "MEST", 4)==0)\
			||(strncmp(TZ, "MESZ", 4)==0)||(strncmp(TZ, "SST", 3)==0)||(strncmp(TZ, "FST", 3)==0)){*pOffsetMin = 1*60;return 13;}// 1
	else if (strncmp(TZ, "EET", 3)==0){*pOffsetMin = 2*60;return 14;}// 2
	else if (strncmp(TZ, "BT", 2)==0){*pOffsetMin = 3*60;return 15;}// 3
	else if (strncmp(TZ, "ZP4", 3)==0){*pOffsetMin = 4*60;return 16;}// 4
	else if (strncmp(TZ, "ZP5", 3)==0){*pOffsetMin = 5*60;return 17;}//5
	else if (strncmp(TZ, "ZP6", 3)==0){*pOffsetMin = 6*60;return 18;}//6
	else if (strncmp(TZ, "ZP7", 3)==0){*pOffsetMin = 7*60;return 19;}//7
	else if (strncmp(TZ, "WAST", 4)==0){*pOffsetMin = 8*60;return 20;}//8
	else if (strncmp(TZ, "JST", 3)==0){*pOffsetMin = 9*60;return 21;}//9
	else if (strncmp(TZ, "ACT", 3)==0){*pOffsetMin = 10*60;return 22;}//10
	else if (strncmp(TZ, "EAST", 4)==0){*pOffsetMin = 11*60;return 23;}//11
	else if (strncmp(TZ, "IDLE", 4)==0){*pOffsetMin = 12*60;return 24;}//12

	else return 100;//ERROR
}

int checkhostname(char *hostname)
{
	char Hostname[1024] = {0};
	gethostname(Hostname, 1023);
	if (strncmp(Hostname, hostname, sizeof(*hostname)) == 0)
	{
		return 0; //No error
	}
	while(*hostname != '\0')
	{
		if(*hostname=='_')
			return 1;
		hostname++;
	}
	return 0; //No error
}

in_addr_t get_netmask_by_prefix_len(unsigned int prefix_len)
{
	unsigned int netmask = 0XFFFFFFFF;

	netmask = netmask << (32 - prefix_len);
	return htonl(netmask);
}

in_addr_t get_gateway_by_prefix_len(unsigned int IP, unsigned int prefix_len)
{
	unsigned int netmask = 0XFFFFFFFF;
	netmask = netmask << (32 - prefix_len);

	IP = (IP & netmask ) + 1;
	
	return htonl(IP);
}

int netsplit( char *pAddress, void *ip )
{
	unsigned int ret;
	NET_IPV4 *ipaddr = (NET_IPV4 *)ip;

	if ((ret = atoi(pAddress + 9)) > 255)
		return FALSE;
	ipaddr->str[3] = ret;

	*( pAddress + 9 ) = '\x0';
	if ((ret = atoi(pAddress + 6)) > 255)
		return FALSE;
	ipaddr->str[2] = ret;

	*( pAddress + 6 ) = '\x0';
	if ((ret = atoi(pAddress + 3)) > 255)
		return FALSE;
	ipaddr->str[1] = ret;

	*( pAddress + 3 ) = '\x0';
	if ((ret = atoi(pAddress + 0)) > 255)
		return FALSE;
	ipaddr->str[0] = ret;

	return TRUE;
}

int ipv4_str_to_num(char *data, struct in_addr *ipaddr)
{
	if ( strchr(data, '.') == NULL )
		return netsplit(data, ipaddr);
	return inet_aton(data, ipaddr);
}

/* @brief Check if IP is valid */
int isValidIp4 (char *str) 
{
	int segs = 0;   /* Segment count. */
	int chcnt = 0;  /* Character count within segment. */
	int accum = 0;  /* Accumulator for segment. */
	/* Catch NULL pointer. */
	if (str == NULL)
		return 0;
	/* Process every character in string. */
	while (*str != '\0') 
	{
		/* Segment changeover. */
		if (*str == '.') 
		{
			/* Must have some digits in segment. */
			if (chcnt == 0) 
				return 0;
			/* Limit number of segments. */
			if (++segs == 4) 
				return 0;
			/* Reset segment values and restart loop. */
			chcnt = accum = 0;
			str ++;
			continue;
		}

		/* Check numeric. */
		if ((*str < '0') || (*str > '9')) 
			return 0;
		/* Accumulate and check segment. */
		if ((accum = accum * 10 + *str - '0') > 255) 
			return 0;
		/* Advance other segment specific stuff and continue loop. */
		chcnt ++;
		str ++;
	}
	/* Check enough segments and enough characters in last segment. */
	if (segs != 3) 
		return 0;
	if (chcnt == 0) 
		return 0;
	/* Address okay. */
	return 1; 
} 

/* @brief Check if a hostname is valid */
int isValidHostname (char *str) 
{
	/* Catch NULL pointer. */
	if (str == NULL) 
	{
		return 0;
	}
	/* Process every character in string. */
	while (*str != '\0') 
	{
		if ((*str >= 'a' && *str <= 'z') || (*str >= 'A' && *str <= 'Z') || (*str >= '0' && *str <= '9') || (*str == '.') || (*str == '-') )
		{
			str++;
		}
		else
		{
			return 0;
		}
	}
	return 1; 
}

int GetNtpServer(char *buf)
{
	TimeConfig timeCfg;
	SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
	if (!pSystemCfg)
	{	
		if(0)//is_Y_Version)//YCX版本
			strcpy(buf, " ");
		else
			strcpy(buf, "time.windows.com");
	}
	else
	{
		memcpy(&timeCfg, &pSystemCfg->timeCfg, sizeof(timeCfg));
		if (strcmp(timeCfg.timeMode.modeName, TIME_MODE_NAME_NTP) == 0)
		{
			strcpy(buf, timeCfg.ntpConfig.serverIP);
			
		}
		
		else
		{
			if(0)//is_Y_Version)//YCX版本
				strcpy(buf, " ");
			else
				strcpy(buf, "time.windows.com");
		}

	}

	return 0;
}

int SetNtpConfig(char *ntpServer)
{
	TimeConfig timeCfg;
	SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
	if (!pSystemCfg)
	{
		return -1;
	}
	memcpy(&timeCfg, &pSystemCfg->timeCfg, sizeof(timeCfg));

	log_print(HT_LOG_INFO, "ntp server:%s\n", ntpServer);

	strcpy(timeCfg.timeMode.modeName, TIME_MODE_NAME_NTP);

	strcpy(timeCfg.ntpConfig.serverIP, ntpServer);

	log_print(HT_LOG_INFO, "mod:%s, serverip:%s\n", timeCfg.timeMode.modeName,timeCfg.ntpConfig.serverIP);

	return anj_config_system_time_set(&timeCfg);
}

unsigned long long g_phyAddr = 0;
void *g_pMappedAddr = NULL;
unsigned int g_mapLen = 0;
unsigned int g_curentDataLen = 0;

void GetOverlayPos(int SetType, char *onvif_pos_type,float onvif_pos_x,float onvif_pos_y, Positiontype *pos_type, int *pos_x, int *pos_y)
{
	if (onvif_pos_type)
	{
		if (strcmp(onvif_pos_type, "UpperLeft") == 0)
		{
			if(pos_type!=NULL)
			{
				*pos_type = POSITION_TYPE_BY_FOUR_CORNER;
				*pos_x = 0;
				*pos_y = 0;
			}
			else
			{
				*pos_x = 0;
				*pos_y = 0;
			}
		}
		else if(strcmp(onvif_pos_type, "UpperRight") == 0)
		{
			if(pos_type!=NULL)
			{
				*pos_type = POSITION_TYPE_BY_FOUR_CORNER;
				*pos_x = 1;
				*pos_y = 0;
			}
			else
			{
				*pos_x = 100;
				*pos_y = 0;
			}
		}
		else if(strcmp(onvif_pos_type, "LowerLeft") == 0)
		{
			if(pos_type!=NULL)
			{
				*pos_type = POSITION_TYPE_BY_FOUR_CORNER;
				*pos_x = 0;
				*pos_y = 1;
			}
			else
			{
				*pos_x = 0;
				*pos_y = 100;
			}
		}
		else if(strcmp(onvif_pos_type, "LowerRight") == 0)
		{
			if(pos_type!=NULL)
			{
				*pos_type = POSITION_TYPE_BY_FOUR_CORNER;
				*pos_x = 1;
				*pos_y = 1;
			}
			else
			{
				*pos_x = 100;
				*pos_y = 100;
			}
		}		
		else if(strcmp(onvif_pos_type, "Custom") == 0)
		{
			if(pos_type!=NULL)
				*pos_type = POSITION_TYPE_BY_SCALE;

			float xscale, yscale;//Add by csj 2019/2/20

			if(SetType == 1)
			{
				xscale = (onvif_pos_x + 1.0)/2;
				if(xscale > 0.75)
					xscale = 0.75;
				
				xscale = xscale/0.75;
			}
			else if(SetType == 2)
			{
				xscale = (onvif_pos_x + 1.0)/2;
				if(xscale > 0.85) 
					xscale = 0.85;
				xscale = xscale/0.85;
			}
			else
				xscale = (onvif_pos_x + 1.0)/2;
			
			yscale = (1.0 - onvif_pos_y)/2;
			if(yscale > 0.95) 
				yscale = 0.95;
			yscale = yscale/0.95;
			
			*pos_x = (int)(xscale*100);
			
			if(*pos_x<0)
				*pos_x=0;
			else if(*pos_x>100)
				*pos_x=100;
			
			*pos_y = (int)(yscale*100);
			
			if(*pos_y<0)
				*pos_y=0;
			else if(*pos_y>100)
				*pos_y=100;
		}
	}
	log_print(HT_LOG_INFO, "onvif_pos_type=%s,onvif_pos_x=%f,onvif_pos_y=%f;pos_x=%d,pos_y=%d", onvif_pos_type,onvif_pos_x,onvif_pos_y,*pos_x,*pos_y);
}

int set_osd(char *token, char * content_format,char *pos_type,float pos_x,float pos_y)
{
	int Settype = -1;
	VideoOverlay config;
	memcpy(&config, &((MediaConfig *)getMediaConfig())->videoConfig[0].overlay, sizeof(config));

	int ret = 0;
	log_print(HT_LOG_INFO, "set_osd token:%s pos_type:%s,pos(%f,%f), content:%s\n", token, pos_type,pos_x,pos_y,content_format);
	//unsigned char text_buf[1024] = {'\0'};
	//unsigned char text_buf_tps[1024] = {'\0'};
	
	if( 0 == strcmp("OSDConfigurationToken_2", token) && content_format)
	{
		if (strcmp(content_format, "yyyy-MM-dd") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "yyyy-mm-dd hh:mm:ss");
		}
		else if(strcmp(content_format, "yyyy/MM/dd") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "yyyy/mm/dd hh:mm:ss");
		}
		else if(strcmp(content_format, "yy-MM-dd") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "yy-mm-dd hh:mm:ss");
		}
		else if(strcmp(content_format, "yy/MM/dd") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "yy/mm/dd hh:mm:ss");
		}
		else if(strcmp(content_format, "dd/MM/yyyy") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "hh:mm:ss dd/mm/yyyy");
		}
		else if(strcmp(content_format, "dd-MM-yyyy") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "hh:mm:ss dd-mm-yyyy");
		}
		else if(strcmp(content_format, "M/d/yyyy") == 0 || strcmp(content_format, "MM/dd/yyyy") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "mm/dd/yyyy hh:mm:ss");
		}
		else if(strcmp(content_format, "MM-dd-yyyy") == 0)
		{
			strcpy(config.timeOverlay.timeFormat.format, "mm-dd-yyyy hh:mm:ss");
		}

		Settype = 1;
		GetOverlayPos(Settype,pos_type,pos_x,pos_y,&config.timeOverlay.posType, &config.timeOverlay.posX, &config.timeOverlay.posY);

		ret =  anj_config_overlay_set(&config, 0);
		if (ret)
			log_print(HT_LOG_INFO, "anj_config_overlay_set fail(%d)\n", ret);
	}
	else if( 0 == strcmp("OSDConfigurationToken_1", token))
	{
		if(content_format!=NULL)
			strncpy(config.titleOverlay.title_utf8, content_format, TITLE_MAX_LEN-1);
		
		Settype = 2;
		GetOverlayPos(Settype,pos_type,pos_x,pos_y,&config.titleOverlay.posType, &config.titleOverlay.posX, &config.titleOverlay.posY);
		ret =  anj_config_overlay_set(&config, 0);
		if (ret)
			log_print(HT_LOG_INFO, "anj_config_overlay_set fail(%d)\n", ret);
	}
	else
	{
		log_print(HT_LOG_INFO, "not support %s\n", token);
		return 0;
	}
	
	return ret;
}

int oset_GetSecFromMonthWeekNo(int year, int month, int weekno, int weekday)
{
	struct tm tbuf;
	struct tm first_day;	//这个月第一天
	first_day.tm_year = year - 1900;
	first_day.tm_mon 	= month - 1;
	first_day.tm_mday = 1;
	first_day.tm_hour = 0;
	first_day.tm_min  = 0;
	first_day.tm_sec  = 0;
	int  timesecond_firstday = mktime(&first_day);//算出来第一天的时间秒,用于计算第几周星期几的具体日期
	memcpy(&first_day, localtime_r((time_t *)&timesecond_firstday, &tbuf), sizeof(struct tm));
	int a = (7 - first_day.tm_wday)%7; //计算第一周如果不完整的话有几天
										//第一天是周日,则是完整的一周。
										//否则第一周只有a天，例如第一天是周六，则第一周只有1天

	//第一周不完整，如果weekday小于1号的星期几，就需要对周数加1
	//weekday=0表示周日，其他表示周一-周六
	if( a > 0 )
	{
		if( weekday < first_day.tm_wday )
		{
			weekno += 1;
//CHAM 20211104改成第几个星期几，而不是第几周的星期几
			log_print(HT_LOG_INFO, "%04d-%02d the first week from weekday %d, set weekday %d to weekno %d \n", 
			year, month, first_day.tm_wday,  weekday, weekno);
//			weekday = first_day.tm_wday;
		}
	}

	int tm_mday = 0;
	tm_mday = (weekno -1 ) * 7 + (weekday - first_day.tm_wday + 1 );
	int nMaxMday;
	switch(month)
	{
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			nMaxMday = 31;
			break;

		case 4:
		case 6:
		case 9:
		case 11:
			nMaxMday = 31;
			break;

		case 2:
		{
			if( year % 4 == 0 )
				nMaxMday = 29;
			else
				nMaxMday = 28;
		}
			break;

		default:
			nMaxMday = 30;			
	}
	
	if( tm_mday > nMaxMday )
		tm_mday = nMaxMday;

	first_day.tm_mday = tm_mday;
	return  mktime(&first_day);
}

static const char base64Char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  

/*
NOte: to free the malloc memory
*/
static char* base64Encode(char const* origSigned, unsigned origLength)   
{
	unsigned char const* orig = (unsigned char const*)origSigned; // in case any input bytes have the MSB set   
	if (orig == NULL) return NULL;

	unsigned const numOrig24BitValues = origLength/3;
	int havePadding = origLength > numOrig24BitValues*3;
	int havePadding2 = origLength == numOrig24BitValues*3 + 2;
	unsigned const numResultBytes = 4*(numOrig24BitValues + havePadding);  
	char* result = (char *)malloc((numResultBytes+1) * sizeof(char)); // allow for trailing '/0'

	// Map each full group of 3 input bytes into 4 output base-64 characters:
	unsigned i;
	for (i = 0; i < numOrig24BitValues; ++i)
	{
		result[4*i+0] = base64Char[(orig[3*i]>>2)&0x3F];
		result[4*i+1] = base64Char[(((orig[3*i]&0x3)<<4) | (orig[3*i+1]>>4))&0x3F];
		result[4*i+2] = base64Char[((orig[3*i+1]<<2) | (orig[3*i+2]>>6))&0x3F];
		result[4*i+3] = base64Char[orig[3*i+2]&0x3F];  
	}

	// Now, take padding into account.  (Note: i == numOrig24BitValues)
	if (havePadding)
	{
		result[4*i+0] = base64Char[(orig[3*i]>>2)&0x3F];
		if (havePadding2)
		{
			result[4*i+1] = base64Char[(((orig[3*i]&0x3)<<4) | (orig[3*i+1]>>4))&0x3F];
			result[4*i+2] = base64Char[(orig[3*i+1]<<2)&0x3F];
		}
		else
		{
			result[4*i+1] = base64Char[((orig[3*i]&0x3)<<4)&0x3F];
			result[4*i+2] = '=';
		}
		result[4*i+3] = '=';
	}

	result[numResultBytes] = '\0';
	return result;
}

unsigned int unpackbits(unsigned char *outp, unsigned char *inp, unsigned int outlen, unsigned int inlen)  
{  
	unsigned int i, len;  
	int val;  

	/* i counts output bytes; outlen = expected output size */  
	for(i = 0; inlen > 1 && i < outlen;){  
		/* get flag byte */  
		len = *inp++;  
		--inlen;  

		if(len == 128) /* ignore this flag value */  
			; // warn_msg("RLE flag byte=128 ignored");  
		else{  
			if(len > 128){  
				len = 1+256-len;  

				/* get value to repeat */  
				val = *inp++;  
				--inlen;  

				if((i+len) <= outlen)  
					memset(outp, val, len);  
				else{  
					memset(outp, val, outlen-i); // fill enough to complete row  
					log_print(HT_LOG_INFO, "unpacked RLE data would overflow row (run)\n");  
					len = 0; // effectively ignore this run, probably corrupt flag byte  
				}  
			}else{  
				++len;  
				if((i+len) <= outlen){  
					if(len > inlen)  
						break; // abort - ran out of input data  
					/* copy verbatim run */  
					memcpy(outp, inp, len);  
					inp += len;  
					inlen -= len;  
				}else{  
					memcpy(outp, inp, outlen-i); // copy enough to complete row  
					log_print(HT_LOG_INFO, "unpacked RLE data would overflow row (copy)\n");  
					len = 0; // effectively ignore  
				}  
			}  
			outp += len;  
			i += len;  
		}  
	}  
	if(i < outlen)  
		log_print(HT_LOG_INFO, "not enough RLE data for row\n");  
	return i;  
}

unsigned int packbits(unsigned char *src, unsigned char *dst, unsigned int n)
{
	unsigned char *p, *q, *run, *dataend;
	int count, maxrun;

	dataend = src + n;
	for( run = src, q = dst; n > 0; run = p, n -= count )
	{
		// A run cannot be longer than 128 bytes.
		maxrun = n < 128 ? n : 128;  
		if(run <= (dataend-3) && run[1] == run[0] && run[2] == run[0])
		{
			// 'run' points to at least three duplicated values.
			// Step forward until run length limit, end of input,
			// or a non matching byte:
			for( p = run+3; p < (run+maxrun) && *p == run[0]; )
				++p;
			count = p - run;

			// replace this run in output with two bytes:   
			*q++ = 1+256-count; /* flag byte, which encodes count (129..254) */

			*q++ = run[0];      /* byte value that is duplicated */

		}
		else
		{
			// If the input doesn't begin with at least 3 duplicated values,
			// then copy the input block, up to the run length limit,
			// end of input, or until we see three duplicated values:
			for( p = run; p < (run+maxrun); )
				if(p <= (dataend-3) && p[1] == p[0] && p[2] == p[0])
					break; // 3 bytes repeated end verbatim run 
				else
					++p;
			count = p - run;
			*q++ = count-1;        /* flag byte, which encodes count (0..127) */
			memcpy(q, run, count); /* followed by the bytes in the run */
			q += count;
		}
	}
	return q - dst;
}

char*  Aj_Get_ActiveCells_str(const MotionDetectAlarm *p_md_alarm)
{
	log_print(HT_LOG_DBG, "Aj_Get_ActiveCells_str\n");
	int nBlockX = (p_md_alarm->blockCount & 0xffff0000) >> 16;
	int nBlockY = p_md_alarm->blockCount & 0x0000ffff;
	
	//log_print(HT_LOG_INFO, "nBlockX:%d, nBlockY:%d\n", nBlockX, nBlockY);
	
	int nBytes = nBlockX * nBlockY / 8;
	if( (nBlockX * nBlockY) % 8 != 0 )
		nBytes += 1;
	
	if( nBytes <= 0 )
		nBytes = 1;
	//log_print(HT_LOG_INFO, "nBytes:%d\n", nBytes);

	int nSize = nBytes * sizeof(unsigned char);
	//log_print(HT_LOG_INFO, "nSize:%d\n", nSize);
	unsigned char *p_bits_activeCells = (unsigned char*)malloc(nSize);
	memset(p_bits_activeCells, 0, nSize);
	
	nSize = 2* nBytes * sizeof(unsigned char);
	//log_print(HT_LOG_INFO, "nSize:%d\n", nSize);
	unsigned char *p_str_activeCells = (unsigned char*)malloc(nSize);
	memset(p_str_activeCells, 0, nSize);
	
	int k = 0;
	for( k = 0; k <= nBytes; k++)
	{
		p_bits_activeCells[k]  = 0;
		if( p_md_alarm->enable > 0 )
		{
			int n = 0;
			for( n = 0; n < 8; n++)
			{
				int r = 7-n;
				int index = k*8 + n;
				if( index > nBlockX * nBlockY )
					break;
				
				char d = p_md_alarm->blockCfg[index] == '1' ? '1':'0';
				p_bits_activeCells[k] +=  (d  - '0') << r;
			}
		}
		//log_print(HT_LOG_INFO, "%d: %#x\n", k, p_bits_activeCells[k]);
	}

/*	char szTmp[4*32];
	szTmp[0] = 0;
	for( k = 0; k < nBytes; k++)
	{
		if( k % 8 == 0  && k != 0)
		{
			log_print(HT_LOG_INFO, "k=%02d, %s\n",k, szTmp);
			szTmp[0] = 0;
		}
		sprintf(szTmp+strlen(szTmp), "%02x ", p_bits_activeCells[k]);
	}

	if( strlen(szTmp ) > 0 )
		log_print(HT_LOG_INFO, "k=%02d, %s\n",k, szTmp);*/

	int packbits_len = packbits(p_bits_activeCells, p_str_activeCells, nBytes);
	//log_print(HT_LOG_INFO, "p_str_activeCells %s\n", p_str_activeCells);

	char *end_to_free = base64Encode((char const *)p_str_activeCells, packbits_len);
	//log_print(HT_LOG_INFO, "p_str_activeCells %s\n", p_str_activeCells);
	free(p_bits_activeCells);
	free(p_str_activeCells);

	return end_to_free;
}

void Aj_Get_ActiveCells(const MotionDetectAlarm *p_md_alarm,      char **Value)
{
	//tan__GetRulesResponse->Rule->Parameters->SimpleItem[3].Value = "zwA="; //J  20140319

	char *end_to_free = Aj_Get_ActiveCells_str(p_md_alarm);
	*Value = end_to_free;//J  20140328

	return ;
}

unsigned char* End_byte( signed char* Count )
{
	unsigned char* Pack = (unsigned char*)(Count+1);
	signed char c = *Count;
	if (c >0)
		Pack = &(Pack[c+1]);
	else
		Pack = &(Pack[1]);
	return Pack;
}

int unPack_count(char Pack[], int count)
{
	int nRes = 0;
	signed char* Count = (signed char*)Pack;
	while ((char*)Count < (Pack+count))
	{
		int c = *Count;
		if (c<0)
			nRes += (1-c);
		else
			nRes += (1+c);
		Count = (signed char*)End_byte(Count);
	}
	return nRes;
}

int tiff6_unPackBits(char Pack[], int count, unsigned char array[] /*= NULL*/)
{
	if (!array) 
		return unPack_count(Pack, count);
	int nRes = 0;
	signed char* Count = (signed char*)Pack;
	while ((char*)Count < (Pack+count))
	{
		int c = *Count;
		if (c<0)
		{
			int n = (1-c);
			memset(&(array[nRes]), Count[1], n);
			nRes += n;
		}
		else
		{
			int n = (1+c);
			memcpy(&(array[nRes]), &Count[1], n);
			nRes += n;
		}
		Count = (signed char*)End_byte(Count);
	}
	return nRes;
}

int get_HB_MDCfgByXml(MotionDetectAlarm *mdCfg, char *xmlBuf)
{
	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if(pDocNode == NULL){
		log_print(HT_LOG_INFO, "xml error\r\n");
		return -1;
	}
	
	IXML_NodeList* pNodelist = NULL;
	pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "tt_SimpleItem");
	if(pNodelist != NULL)
	{	
		IXML_Node* tmpAttr = NULL;
		tmpAttr = pNodelist->nodeItem->firstAttr;
		int isMDEnable=0;
		while(tmpAttr)
		{
			if(!strcmp(tmpAttr->nodeName, "Name"))
			{
				if(!strcmp(tmpAttr->nodeValue, "MotionDetectorEnable")){
					isMDEnable=1;
				}
			}
			tmpAttr = tmpAttr->nextSibling;
		}
		tmpAttr = pNodelist->nodeItem->firstAttr;
		while(tmpAttr)
		{
			if(!strcmp(tmpAttr->nodeName, "Value"))
			{
				if((!strcmp(tmpAttr->nodeValue, "true")) && isMDEnable){
					mdCfg->enable=1;
					//g_motion_mode = 0;
				}
				else if((!strcmp(tmpAttr->nodeValue, "false")) && isMDEnable){
					//if(is_Y_Version == 0)
						mdCfg->enable=0;
					//g_motion_mode = 1;
				}
			}
			tmpAttr = tmpAttr->nextSibling;
		}	
		ixmlNodeList_free(pNodelist);
	}
	
	TimeSpanList timeSpanList;
	pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "tt_ElementItem");
	IXML_NodeList* pNodelist_bak = NULL;
	int WDay_Cnt = 0;
	if(pNodelist != NULL)
	{
		pNodelist_bak = pNodelist;
		while(pNodelist)
		{
			WDay_Cnt++;
			pNodelist=pNodelist->next;
		}
		memset(&timeSpanList,0,sizeof(timeSpanList));
		timeSpanList.workdayCnt=WDay_Cnt;
		ixmlNodeList_free(pNodelist_bak);
	}
	
	pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "tt_ElementItem");
	pNodelist_bak = pNodelist;
	int WDayIndex=0;

	while(WDay_Cnt != 0 && pNodelist)
	{	
		IXML_Node* tmpAttr = NULL;
		tmpAttr = pNodelist->nodeItem->firstAttr;
		while(tmpAttr)
		{
			if(!strcmp(tmpAttr->nodeName, "Value"))
			{
				if(!strcmp(tmpAttr->nodeValue, "Sunday")){
					timeSpanList.workdayTimes[WDayIndex].workday=0;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Monday")){
					timeSpanList.workdayTimes[WDayIndex].workday=1;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Tuesday")){
					timeSpanList.workdayTimes[WDayIndex].workday=2;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Wednesday")){
					timeSpanList.workdayTimes[WDayIndex].workday=3;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Thursday")){
					timeSpanList.workdayTimes[WDayIndex].workday=4;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Friday")){
					timeSpanList.workdayTimes[WDayIndex].workday=5;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Saturday")){
					timeSpanList.workdayTimes[WDayIndex].workday=6;
				}
				else if(!strcmp(tmpAttr->nodeValue, "Everyday")){
					timeSpanList.workdayTimes[WDayIndex].workday=7;
				}
				else{
					timeSpanList.workdayTimes[WDayIndex].workday=7;
				}
			}
			tmpAttr = tmpAttr->nextSibling;
		}
		IXML_Node *tmpChild;
		tmpChild = pNodelist->nodeItem->firstChild;
		int time_Cnt = 0;
		while(tmpChild)
		{
			if(!strcmp(tmpChild->nodeName, "tt_SimpleItem"))
				time_Cnt++;
			tmpChild=tmpChild->nextSibling;
		}
		timeSpanList.workdayTimes[WDayIndex].timeSpancnt=time_Cnt;

		tmpChild = pNodelist->nodeItem->firstChild;
		int timeIndex=0;
		while(tmpChild)
		{
			if(!strcmp(tmpChild->nodeName, "tt_SimpleItem"))
			{
				IXML_Node* tmpAttr=tmpChild->firstAttr;
				while(tmpAttr)
				{
					if(!strcmp(tmpAttr->nodeName, "Value"))
					{
						//Value="00:00:00-08:00:00"
						int startH=0,startM=0,startS=0,endH=0,endM=0,endS=0;
						sscanf(tmpAttr->nodeValue,"%d:%d:%d-%d:%d:%d",&startH,&startM,&startS,&endH,&endM,&endS);
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].startTime.hour=startH;
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].startTime.minute=startM;
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].startTime.sec=startS;
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].endTime.hour=endH;
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].endTime.minute=endM;
						timeSpanList.workdayTimes[WDayIndex].timeSpans[timeIndex].endTime.sec=endS;
					}
					tmpAttr = tmpAttr->nextSibling;
				}
				timeIndex++;
			}
			tmpChild = tmpChild->nextSibling;
		}

		WDayIndex++;
		pNodelist=pNodelist->next;
	}
	if(pNodelist_bak)
		ixmlNodeList_free(pNodelist_bak);

	if(pDocNode)
		ixmlDocument_free(pDocNode);

	//当WDay_Cnt==0时，然后改为每天 00:00:00-23:59:59
	if(WDay_Cnt==0){
		memset(&timeSpanList,0,sizeof(timeSpanList));
		timeSpanList.workdayCnt = 1;
		timeSpanList.workdayTimes[0].workday = 7;//每天
		timeSpanList.workdayTimes[0].timeSpancnt = 1;
		timeSpanList.workdayTimes[0].timeSpans[0].startTime.hour=0;
		timeSpanList.workdayTimes[0].timeSpans[0].startTime.minute=0;
		timeSpanList.workdayTimes[0].timeSpans[0].startTime.sec=0;
		timeSpanList.workdayTimes[0].timeSpans[0].endTime.hour=23;
		timeSpanList.workdayTimes[0].timeSpans[0].endTime.minute=59;
		timeSpanList.workdayTimes[0].timeSpans[0].endTime.sec=59;
	}

	TransTimeSpan2New(&timeSpanList, &mdCfg->timeSpan);	
	return 0;
}

int get_xml_value_Date(char *buf, char *node_start, char *node_end, char *dest)
{
	char *buf1 = strstr(buf, node_start);
	char start[128] = {0};
	sprintf(start, "<%s>", node_start);
	char end[128] = {0};
	sprintf(end, "</%s>", node_end);
	char *str1 = strstr(buf, start);
	char *str2 = strstr(buf1, end);
	if (str1 && str2)
	{
		int len = strlen(start);
		memcpy(dest, str1 + len, str2 - str1 - len + strlen("</tt:ElementItem>"));
	}
	return 0;
}

//获取天的时间段个数
int Get_Elementltem_Date_Num(char *Msg, int *flag)
{
	char *head = NULL;
	char *tail = NULL;
	head = strstr(Msg, "time_seg");
	tail = strstr(Msg, " </tt");
	int num = 0;
	while (head != tail)
	{
		if (strstr(head, "time_seg") != NULL)
		{
			head = strstr(head, "time_seg");
			num++;
		}
		else
			break;
		head ++;
	}
	*flag = num;
	
	return 0;
}

int get_xml_value_date(char *buf, char *dest, int flag)
{
	char start[128] = {0};
	if (flag == 0)
		strcpy(start, "Value=\"");
	else
		strcpy(start, "00-");
	char end[128] = {"\""};
	char *str1 = strstr(buf, start);
	str1 += sizeof("Value=\"") -1 ;
	char *str2 = strstr(str1, end);
	if (str1 && str2)
	{
		int len = strlen(start) -sizeof("Value=\"");
		memcpy(dest, str1 + len + 1, 2);
	}
	return 0;
}

//获取时间段数值
int Get_ElementItem_Date_Value(char *Msg, int **xy_arry, int x_num, int y_num)
{
	char *x_add = NULL;
	char *y_add = NULL;
	x_add = strstr(Msg, "Value=");
	y_add = strstr(Msg, "00-");
	int i = 0;
	for (i = 0; i < x_num; i++)
	{
		char x[32] = {0};
		char y[32] = {0};
		get_xml_value_date(x_add, x, 0);
		get_xml_value_date(y_add, y, 1);
		*((int *)xy_arry + i*y_num + 0) = atoi(x);
		*((int *)xy_arry + i*y_num + 1) = atoi(y);
		x_add++;
		y_add++;
		x_add = strstr(x_add, "Value=");
		y_add = strstr(y_add, "00-");
	}
	
	return 0;
}

int Set_date(char *Date_msg, int flag, PdAlarm *pAlarmt)
{
	int i, j;
	//PdAlarm pAlarmt;
	//MsgGetPdAlarm(&pAlarmt);
	//log_print(HT_LOG_INFO, "Date_msg:%s\n", Date_msg);
	int Num;
	Get_Elementltem_Date_Num(Date_msg, &Num);
	int date[Num][2];
	Get_ElementItem_Date_Value(Date_msg, (int**)date, Num, 2);
	//for (i = 0; i < Num; i++)
	//{
		//log_print(HT_LOG_INFO, "date:%d~%d\n", date[i][0], date[i][1]+1);
	//}
	//log_print(HT_LOG_INFO, "pAlarmt.timeSpan.workday[%d]:%d\n", flag, pAlarmt.timeSpan.workday[flag]);
	pAlarmt->timeSpan.workday[flag] = 0;
	//log_print(HT_LOG_INFO, "pAlarmt.timeSpan.workday[%d]:%d\n", flag, pAlarmt.timeSpan.workday[flag]);
	for (i = 0; i < Num; i++)
	{
		for (j = date[i][0]; j <= date[i][1]; j++)
		{
			pAlarmt->timeSpan.workday[flag] += (1<<j);
		}
	}
	
	//log_print(HT_LOG_INFO, "pAlarmt.timeSpan.workday[%d]:%d\n", flag, pAlarmt.timeSpan.workday[flag]);
	//MsgSetPdAlarm(&pAlarmt);
	return 0;
}

int get_xml_valueCoordinate(char *buf, char *dest, int flag)
{
	char start[128] = {0};
	if (flag == 0)
		strcpy(start, "x=\"");
	else
		strcpy(start, "y=\"");
	char end[128] = {"\""};
	char *str1 = strstr(buf, start);
	str1 += sizeof("x=\"") -1 ;
	char *str2 = strstr(str1, end);
	if (str1 && str2)
	{
		int len = strlen(start) -sizeof("x=\"");
		memcpy(dest, str1 + len + 1, str2 - str1 - len );
	}
	return 0;
}

int Get_ElementItem_Coordinate_Value(char *Msg, int **xy_arry, int x_num, int y_num)
{
	char *x_add = NULL;
	char *y_add = NULL;
	x_add = strstr(Msg, "x=");
	y_add = strstr(Msg, "y=");
	int i = 0;
	for (i = 0; i < x_num; i++)
	{
		char x[32] = {0};
		char y[32] = {0};
		get_xml_valueCoordinate(x_add, x, 0);
		get_xml_valueCoordinate(y_add, y, 1);
		*((int *)xy_arry + i*y_num + 0) = atoi(x);
		*((int *)xy_arry + i*y_num + 1) = atoi(y);
		x_add++;
		y_add++;
		x_add = strstr(x_add, "x=");
		y_add = strstr(y_add, "y=");
	}
	
	return 0;
}

int Get_Elementltem_Coordinate_Num(char *Msg, int *flag)
{
	char *head = NULL;
	char *tail = NULL;
	head = strstr(Msg, "x=");
	tail = strstr(Msg, " </tt");
	int num = 0;
	while (head != tail)
	{
		if (strstr(head, "x=") != NULL)
		{
			head = strstr(head, "x=");
			num++;
		}
		else
			break;
		head ++;
	}
	*flag = num;
	
	return 0;
}

int get_https_port(void)
{
	int web_port = gStreamCfg.webConfig.httpsPort;
	if(web_port <= 0  || web_port >= 65535)
		return 443;
	else
		return web_port;
}

int get_url_http(char *first_value, char *url)
{
	char * post_head = first_value;
	char * post_tail = strstr(first_value, " HTTP");
	if (post_head == NULL || post_tail == NULL)
	{
		return -1;
	}
	memcpy(url, post_head, strlen(post_head) - strlen(post_tail));
	
	return 0;
}

