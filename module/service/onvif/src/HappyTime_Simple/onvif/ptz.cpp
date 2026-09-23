#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "ptz_tour.h"
#include "ptz.h"
// ipc1 headers replaced by ipc2 compat shim
// #include "msg_def.h"
// #include "aux_msg.h"
// #include "sys_info.h"
#include "onvif_compat_shim.h"



#define FLAG_CONTINUE_UNTIL_STOP (255)//J differ from phone controling 20140923
#define ONVIF_PTZ_PRESET_SAVEFILE "/mnt/nand/onvif_ptz.cfg"

ptz_preset_list *g_ptz_list = NULL;
static pthread_mutex_t m_mutex;

int g_ptz_flag_list[255] = {0};

void ptz_mutex_init()
{
	pthread_mutex_init( &m_mutex, NULL );
	//mysystem("rm -fr /mnt/nand/onvif_ptz.cfg");
}

void ptz_mutex_lock()
{
	pthread_mutex_lock( &m_mutex );
}

void ptz_mutex_unlock()
{
	pthread_mutex_unlock( &m_mutex );
}

void ptz_mutex_deinit()
{
	pthread_mutex_destroy( &m_mutex );
}


int atoi_plus(char *str)
{
	if(str==NULL)
		return 0;
	char *pIndex = str;
	int len = strlen(str);

	while(len){
		if(((*pIndex)>='0')&&((*pIndex)<='9'))
			break;
		pIndex++;
		len--;
	}

	if(len==0)
		return 0;

	return atoi(pIndex);
}

int save_onvif_ptz(void)
{
	int ret;
	int presetCount=0;
	
	FILE *fp=fopen(ONVIF_PTZ_PRESET_SAVEFILE,"wb");
	if(NULL == fp)
	{
		HTTP_LOG("open %s failed, err = %s\n", ONVIF_PTZ_PRESET_SAVEFILE, strerror(errno));
		return -1;
	}

	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;
	
	while(pEntry)
	{
		HTTP_LOG("%03d: profile: %s, name: %s, token: %s, num: %d, home: %d\n",
			presetCount++,
			pEntry->profileToken,
			pEntry->presetName,
			pEntry->presetToken,
			pEntry->presetNumber,
			pEntry->isHome);
		
		ret = fwrite(pEntry, 1, sizeof(ptz_preset_list), fp);
		if(ret < (int)sizeof(ptz_preset_list))
		{
			HTTP_LOG("write %s failed, err = %s\n", ONVIF_PTZ_PRESET_SAVEFILE, strerror(errno));
			break;
		}

		pEntry=pEntry->next;
	}
	ptz_mutex_unlock();
	
	fclose(fp);
	return 0;
}

int load_onvif_ptz(void)
{
	FILE *fp=fopen(ONVIF_PTZ_PRESET_SAVEFILE,"rb");
	if(NULL == fp)
	{
		HTTP_LOG("open %s failed, err = %s\n", ONVIF_PTZ_PRESET_SAVEFILE, strerror(errno));
		return -1;
	}
	
	ptz_mutex_lock();
	ptz_preset_list *pEntry = g_ptz_list;
	ptz_preset_list *tmpEntry = NULL;
	int ret;
	int presetCount=0;
	
	while(pEntry)
	{
		tmpEntry = pEntry->next;
		free(pEntry);
		
		pEntry=tmpEntry;
	}
	
	g_ptz_list = NULL;
	
	while(1)
	{
		pEntry = (ptz_preset_list *)malloc(sizeof(ptz_preset_list));
		if(pEntry)
		{
			ret = fread(pEntry, 1, sizeof(ptz_preset_list), fp);
			if(ret < (int)sizeof(ptz_preset_list))
			{
				HTTP_LOG("read %s failed, err = %s\n", ONVIF_PTZ_PRESET_SAVEFILE, strerror(errno));
				break;
			}
			else
			{
				g_ptz_flag_list[pEntry->presetNumber] = 1;
			}
			HTTP_LOG("%03d: profile: %s, name: %s, token: %s, num: %d, home: %d\n",
				presetCount++,
				pEntry->profileToken,
				pEntry->presetName,
				pEntry->presetToken,
				pEntry->presetNumber,
				pEntry->isHome);
			
			pEntry->next=NULL;
			if(g_ptz_list == NULL)
				g_ptz_list = pEntry;
			else
				tmpEntry->next = pEntry;
			
			tmpEntry = pEntry; //remember the last entry
		}
		else
		{
			HTTP_LOG("no memory!!!\n");
			break;
		}
	}
	ptz_mutex_unlock();
	
	fclose(fp);
	return 0;
}

int PtzCmdHandle(int cmd, int panSpeed,int tiltSpeed,int presetId)
{
	static int m_ptz_need_stop = 0;
	
	char msgBody[256]="";
	
	if(panSpeed  == 11)//J  20140728
		panSpeed = 10;
	else if(panSpeed < 0  || panSpeed > 10)
		panSpeed = 5;
	
	if(tiltSpeed == 11)
		tiltSpeed = 10;
	else if(tiltSpeed < 0 || tiltSpeed > 10)
		tiltSpeed = 5;

	if(LENS_FAR <= cmd && cmd <= LENS_DIAPHRAGM_SMALL && m_ptz_need_stop == 1)
		return 0;
	
	switch(cmd)
	{
		case LENS_UP:
			sprintf(msgBody,
			"<xml>\r\n"
			"<cmd>up</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_LEFT:
			sprintf(msgBody, 
			"<xml>\r\n"
			//"<cmd>left</cmd>\r\n"
			"<cmd>left</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;						
			break;
		case LENS_RIGHT:
			sprintf(msgBody, 
			"<xml>\r\n"
			//"<cmd>right</cmd>\r\n"
			"<cmd>right</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_DOWN:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>down</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_LEFT_UP:			
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>left_up</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_LEFT_DOWN:
			sprintf(msgBody, 
			"<xml>\r\n"
			//"<cmd>left</cmd>\r\n"
			"<cmd>left_down</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;						
			break;
		case LENS_RIGHT_UP:
			sprintf(msgBody, 
			"<xml>\r\n"
			//"<cmd>right</cmd>\r\n"
			"<cmd>right_up</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_RIGHT_DOWN:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>right_down</cmd>\r\n"
			"<panspeed>%d</panspeed>\r\n"
			"<tiltspeed>%d</tiltspeed>\r\n"
			"<flag>%d</flag>\r\n"
			"</xml>\r\n", panSpeed, tiltSpeed, FLAG_CONTINUE_UNTIL_STOP);
			//m_ptz_need_stop = 1;
			break;
		case LENS_NEAR://LENS_FAR:
			sprintf(msgBody,
			"<xml>\r\n"
			"<cmd>zoomwide</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;
		case LENS_FAR://NEAR:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>zoomtele</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;
		case LENS_FOCUSNEAR:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>FocusNearAutoOff</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;
		case LENS_FOCUSFAR:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>FocusFarAutoOff</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;			
		case LENS_DIAPHRAGM_LARGE:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>IrisOpenAutoOff</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;
		case LENS_DIAPHRAGM_SMALL:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>IrisCloseAutoOff</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 1;
			break;

		case LENS_PRESET_GOTO:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>callpreset</cmd>\r\n"
			"<preset>%d</preset>\r\n"
			"</xml>\r\n", presetId);
			break;
		case LENS_PRESET_SET:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>setpreset</cmd>\r\n"
			"<preset>%d</preset>\r\n"
			"</xml>\r\n", presetId);
			break;
		case LENS_PRESET_DEL:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>clearpreset</cmd>\r\n"
			"<preset>%d</preset>\r\n"
			"</xml>\r\n", presetId);
			break;

		case LENS_LIGHT_ON:
			sprintf(msgBody,
			"<xml>\r\n"
			"<cmd>ILLOn</cmd>\r\n"
			"</xml>\r\n");
			break;
		case LENS_LIGHT_OFF:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>ILLOff</cmd>\r\n"
			"</xml>\r\n");
			break;
		case LENS_AUTO:
			break;
		case LENS_STOP:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>stop</cmd>\r\n"
			"</xml>\r\n");
			m_ptz_need_stop = 0;
			break;
		//20120427
		case WIPER_PWRON:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>Wiper1On</cmd>\r\n"
			"</xml>\r\n");
			break;
		case WIPER_PWROFF:
			sprintf(msgBody, 
			"<xml>\r\n"
			"<cmd>Wiper1Off</cmd>\r\n"
			"</xml>\r\n");
			break;
		default:
			break;
	}

	int runtime = 1;

	if(cmd == LENS_STOP)
		runtime = 5;

	while((runtime--) > 0)
	{
		if(strlen(msgBody))
			AuxMsgPTZCmd(msgBody);

		if(runtime > 0)
			usleep(50*1000);
	}
	
	//added by XXX 20130327
	if(cmd == LENS_PRESET_SET || cmd == LENS_PRESET_DEL)
		save_onvif_ptz();
	
	return 0;
}

int find_free_preset(int num)
{
	int i;

	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;
	int found = 0;

	if(-1 != num)
	{
		while(pEntry)
		{
			if(pEntry->presetNumber == num)
			{
				found=1;
				break;
			}
			
			pEntry=pEntry->next;
		}
		
		if(!found)
		{
			ptz_mutex_unlock();
			return num;
		}
	}

	
	for(i=1;i<=255;i++)
	{
		pEntry=g_ptz_list;	
		found = 0;
		while(pEntry)
		{
			if(pEntry->presetNumber == i)
			{
				found=1;
				break;
			}
			
			pEntry=pEntry->next;
		}
		
		if(!found)
		{
			ptz_mutex_unlock();
			return i;
		}
	}
	
	ptz_mutex_unlock();
	return -1;	
}
#if 0
int find_free_preset(void)
{
	int i;
	for(i=1;i<256;i++)
	{
		ptz_preset_list *pEntry=g_ptz_list;	
		int found = 0;
		while(pEntry)
		{
			if(pEntry->presetNumber == i)
			{
				found=1;
				break;
			}
			
			pEntry=pEntry->next;
		}
		
		if(!found)
			return i;
	}
	
	return -1;	
}
#endif
void show_preset_list(void)
{
	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;
	int presetCount = 0;
	while(pEntry)
	{
		HTTP_LOG("%03d: profile: %s, name: %s, token: %s, num: %d, home: %d\n",
			presetCount,
			pEntry->profileToken,
			pEntry->presetName,
			pEntry->presetToken,
			pEntry->presetNumber,
			pEntry->isHome);
		
		pEntry=pEntry->next;
		presetCount++;
	}	
	ptz_mutex_unlock();
}

void add_preset_list(ptz_preset_list *pEntry, int bLock)
{
	if(bLock)
		ptz_mutex_lock();

	if(g_ptz_list == NULL || pEntry == NULL)
	{
		if(bLock)
			ptz_mutex_unlock();
		
		return;
	}
	
	ptz_preset_list *last_preset = NULL;
	ptz_preset_list *cur_preset = g_ptz_list;

	pEntry->next = NULL;

	while(cur_preset)
	{
		if(pEntry->presetNumber <= cur_preset->presetNumber)
		{
			if(last_preset == NULL)
			{
				pEntry->next = cur_preset;
				g_ptz_list = pEntry;
			}else 
			{
				last_preset->next = pEntry;
				pEntry->next = cur_preset;
			}
			
			break;
		}

		last_preset = cur_preset;
		cur_preset = cur_preset->next;
	}

	if(cur_preset == NULL && last_preset != NULL)
		last_preset->next = pEntry;

	if(bLock)
		ptz_mutex_unlock();
}


int find_same_preset(char *profileToken, int presetNumber, ptz_preset_list **ppEntry, ptz_preset_list **ppTail)
{
	ptz_preset_list *pEntry = g_ptz_list;
	ptz_preset_list *pTail = NULL;
	
	ptz_mutex_lock();
	while(pEntry)
	{
		if(!strcmp(pEntry->profileToken, profileToken))
		{
			if(pEntry->presetNumber==presetNumber)
			{
				*ppEntry = pEntry;
				ptz_mutex_unlock();
				return 2;
			}
		}

		pTail = pEntry;
		pEntry=pEntry->next;
	}

	*ppTail = pTail;
	
	ptz_mutex_unlock();

	return 0;
}

int add_preset(char *profileToken, char *presetName_onvif, char *presetToken_onvif, char *responseToken, int isHome)
{
	int found=0;
	int presetNumber=0;
	char presetStr[4]={0};

	int bAddOne = 0;//csj 2018/11/7
	
	ptz_preset_list *pTail=NULL;
	ptz_preset_list *pEntry=NULL;

	char *presetName = presetName_onvif;
	char *presetToken = presetToken_onvif;
	char presetName_model[64] = {0};
	char presetToken_model[64] = {0};

	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}

	if(!presetToken && !presetName)
	{
		HTTP_LOG("presetToken & presetName is NULL!!! ready to split\n");
		int i;
		int set_num = -1;
		
		for (i = 0; i < 255; i ++)
		{
			if (g_ptz_flag_list[i] == 0)
			{
				set_num = i;
				break;
			}
		}
		
		if (set_num == -1)
			set_num = 1;
		sprintf(presetName_model, "PresetName%d", set_num);
		sprintf(presetToken_model, "PresetToken%d", set_num);
		
		presetName = presetName_model;
		presetToken = presetToken_model;
		
		HTTP_LOG("presetName:%s, presetToken:%s\n", presetName, presetToken);
	}

	if(presetToken && presetName)//csj 2018/11/7
	{
		if(strlen(presetName) && strstr(presetName, "Preset"))
		{
			if(strlen(presetToken) && !strstr(presetToken, "Preset"))
			{
				HTTP_LOG("presetNum add one\n");
				bAddOne = 1;
			}
		}
	}
	
	if(!presetToken)
	{
		if(presetName[0] == '#')
			presetToken = presetName+1;
		else
			presetToken = presetName;
	}
	else if(presetToken && strlen(presetToken) == 0 && strlen(presetName) != 0)
		presetToken = presetName;
	
	if(presetToken)
	{
		char *strNum=NULL;
		if(strcasestr(presetToken,"Preset")){
			strNum = strcasestr(presetToken,"Preset")+6;
		}
		else{
			strNum = presetToken;
		}
		
		if(strNum){
			int presetNum = atoi_plus(strNum);
			if(bAddOne)//csj 2018/11/7
				presetNum += 1;
			if(presetNum>=1 && presetNum<=255){
				presetNumber=presetNum;
				sprintf(presetStr,"%d",presetNum);
				presetToken=presetStr;
			}
		}
	}

	HTTP_LOG("presetNumber=%d\n", presetNumber);

	found = find_same_preset(profileToken, presetNumber, &pEntry, &pTail);

	if(1 == found)
	{
		HTTP_LOG("found existed one!!!");

		strcpy(responseToken, pEntry->presetToken);	
		PtzCmdHandle(LENS_PRESET_SET, 0, 0, pEntry->presetNumber);
		return 0;
	}
	else if (2 == found)	//cover the ptz
	{
		HTTP_LOG("overwrite existed preset name!!!");
		
		strcpy(responseToken, pEntry->presetToken);
		
		if(presetName && strlen(presetName))
			strcpy(pEntry->presetName, presetName);
			
		PtzCmdHandle(LENS_PRESET_SET, 0, 0, pEntry->presetNumber);
		return 0;
	}
	else//add new one
	{
		pEntry = (ptz_preset_list *)malloc(sizeof(ptz_preset_list));
		memset(pEntry, 0, sizeof(ptz_preset_list));

		if(presetNumber > 0 && presetNumber < 256)
		{
			pEntry->presetNumber = presetNumber;
		}
		else{
			presetToken=(char *)"";
			pEntry->presetNumber = find_free_preset(-1);
		}
		
#if 0
		//XXX 20131018
		int preset_no1 = -1;
		int preset_no2 = -1;
		if(presetName && presetToken)
		{
			if(strstr(presetName, "preset") || strstr(presetName, "Preset")) //20131123
				preset_no1 = atoi(presetName + strlen("preset"));
			else
				preset_no1 = atoi(presetName);
			
			if(strstr(presetToken, "preset") || strstr(presetToken, "Preset"))//20131123
				preset_no2 = atoi(presetToken + strlen("preset"));
			else
				preset_no2 = atoi(presetToken);
		}
		else if(presetName)
		{
			if(strstr(presetName, "preset") || strstr(presetName, "Preset"))//20131123
				preset_no1 = atoi(presetName + strlen("preset"));
			else
				preset_no1 = atoi(presetName);
		}
		else if(presetToken)
		{
			if(strstr(presetToken, "preset") || strstr(presetToken, "Preset"))//20131123
				preset_no2 = atoi(presetToken + strlen("preset"));
			else
				preset_no2 = atoi(presetToken);
		}
		
		HTTP_LOG("preset_no1=%d, preset_no2 = %d\n", preset_no1, preset_no2);
		
		
		if(preset_no1 > 64 && preset_no1 < 256)
		{
			pEntry->presetNumber = preset_no1;
		}
		else if(preset_no2 > 64 && preset_no2 < 256)
		{
			pEntry->presetNumber = preset_no2;
		}
		else
		{
			if(preset_no1 > -1 && preset_no1 <= 64)
			{
				pEntry->presetNumber = find_free_preset(preset_no1);
			}
			else if(preset_no2 > -1 && preset_no2 <= 64)
			{
				pEntry->presetNumber = find_free_preset(preset_no2);
			}
			else
			{
				pEntry->presetNumber = find_free_preset(-1);
			}
		}
		//XXX 20131018
#endif		
		
		if(pEntry->presetNumber < 0)
		{
			HTTP_LOG("too many preset!!!\n");

			free(pEntry);
			return -2;
		}
				
		strcpy(pEntry->profileToken, profileToken);
		pEntry->isHome = isHome;
		pEntry->next   = NULL;

		if(presetToken && strlen(presetToken))
			strcpy(pEntry->presetToken, presetToken);
		
		if(presetName && strlen(presetName))
			strcpy(pEntry->presetName, presetName);
		
		if(strlen(pEntry->presetToken) && !strlen(pEntry->presetName))
			strcpy(pEntry->presetName, pEntry->presetToken);
		else if(strlen(pEntry->presetName) && !strlen(pEntry->presetToken))
			sprintf(pEntry->presetToken,"%d", pEntry->presetNumber);
		else if(!strlen(pEntry->presetName) && !strlen(pEntry->presetToken))
		{
			if(0 <= pEntry->presetNumber && 100 > pEntry->presetNumber)
			{
				sprintf(pEntry->presetName,"Preset%03d", pEntry->presetNumber);//J  20140620
				sprintf(pEntry->presetToken,"%d", pEntry->presetNumber);
			}
			else
			{
				sprintf(pEntry->presetName,"Preset%d", pEntry->presetNumber);
				sprintf(pEntry->presetToken,"%d", pEntry->presetNumber);
			}
		}

		if(bAddOne)//csj 2018/11/7
		{
			sprintf(pEntry->presetName,"Preset%03d", pEntry->presetNumber);
			sprintf(pEntry->presetToken,"%d", pEntry->presetNumber);
		}

		strcpy(responseToken, pEntry->presetToken);

		ptz_mutex_lock();
		if(pTail == NULL)
		{
			HTTP_LOG("add head: prifile: %s, name: %s, token: %s, num: %d\n",
				pEntry->profileToken,
				pEntry->presetName,
				pEntry->presetToken,
				pEntry->presetNumber);
				
			g_ptz_list = pEntry;
		}
		else
		{
			HTTP_LOG("add tail: prifile: %s, name: %s, token: %s, num: %d\n",
				pEntry->profileToken,
				pEntry->presetName,
				pEntry->presetToken,
				pEntry->presetNumber);
				
			//pTail->next = pEntry;
			add_preset_list(pEntry, 0);
		}
		ptz_mutex_unlock();
		
		PtzCmdHandle(LENS_PRESET_SET, 0, 0, pEntry->presetNumber);

		return 0;
	}
}

int goto_preset(char *profileToken, char *presetToken)
{
	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}
	
	if(!presetToken)
	{
		HTTP_LOG("presetToken is NULL!!!\n");
		return -1;
	}
	
	char *strNum=presetToken;
	if(strcasestr(presetToken,"Preset")){
		strNum = strcasestr(presetToken,"Preset")+6;
	}
	
	int presetNum = atoi_plus(strNum);
	
	if( presetNum > 0 )
	{
		PtzCmdHandle(LENS_PRESET_GOTO, 0, 0, presetNum);
		return 0;
	}
	else
	{
		HTTP_LOG("Cannot detect preset number from presetToken %s", presetToken);
		return -1;
	}
}

int remove_preset(char *profileToken, char *presetToken)
{
	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}
	
	if(!presetToken)
	{
		HTTP_LOG("presetToken is NULL!!!\n");
		return -1;
	}

	char *strNum=presetToken;
	if(strcasestr(presetToken,"Preset")){
		strNum = strcasestr(presetToken,"Preset")+6;
	}
	
	int presetNum = atoi_plus(strNum);
	
	if(presetNum < 0)
	{
		HTTP_LOG("Cannot detect preset number from presetToken %s", presetToken);
		return -1;
	}

	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;
	ptz_preset_list *pPrev=g_ptz_list;

	while(pEntry)
	{
		if(!strcmp(pEntry->profileToken, profileToken))
		{
			if(pEntry->presetNumber == presetNum)
			{
				HTTP_LOG("found existed one, presetNumber = %d!!!", pEntry->presetNumber);
				if(pEntry == g_ptz_list){
					HTTP_LOG("remove the header!!!\n");
					g_ptz_list = pEntry->next;
				}
				else{
					HTTP_LOG("remove the tailer!!!\n");
					pPrev->next = pEntry->next;
				}
				free(pEntry);
				ptz_mutex_unlock();
				PtzCmdHandle(LENS_PRESET_DEL, 0, 0, presetNum);
				return 0;
			}
			if(!strcmp(pEntry->presetToken, presetToken))
			{
				HTTP_LOG("found existed one, presetNumber = %d!!!", pEntry->presetNumber);
				if(pEntry == g_ptz_list){
					HTTP_LOG("remove the header!!!\n");
					g_ptz_list = pEntry->next;
				}
				else{
					HTTP_LOG("remove the tailer!!!\n");
					pPrev->next = pEntry->next;
				}
				free(pEntry);
				ptz_mutex_unlock();
				PtzCmdHandle(LENS_PRESET_DEL, 0, 0, presetNum);				
				return 0;
			}
			
		}
		
		pPrev=pEntry;
		pEntry=pEntry->next;
	}
	
	ptz_mutex_unlock();
	HTTP_LOG("token: %s not found!!!\n", presetToken);
	return -1;
}

int set_home_preset(char *profileToken)
{
	char presetToken[256]="";
	
	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}

	ptz_mutex_lock();	
	ptz_preset_list *pEntry=g_ptz_list;
	while(pEntry)
	{
		if(!strcmp(pEntry->profileToken, profileToken) && pEntry->isHome)
		{			
			HTTP_LOG("found existed home preset!!!");
					
			PtzCmdHandle(LENS_PRESET_SET, 0, 0, pEntry->presetNumber);
			ptz_mutex_unlock();
			
			return 0;
		}
		
		pEntry=pEntry->next;
	}
	ptz_mutex_unlock();
	
	HTTP_LOG("set new home position!!!\n");
	
	return add_preset(profileToken, NULL, NULL, presetToken, 1);
}

int goto_home_preset(char *profileToken)
{
			
	if(!profileToken)
	{
		HTTP_LOG("profileToken is NULL!!!\n");
		return -1;
	}
		
	ptz_mutex_lock();
	ptz_preset_list *pEntry=g_ptz_list;
	while(pEntry)
	{
		if(!strcmp(pEntry->profileToken, profileToken) && pEntry->isHome)
		{		
			HTTP_LOG("found home preset, presetNumber = %d!!!", pEntry->presetNumber);
				
			PtzCmdHandle(LENS_PRESET_GOTO, 0, 0, pEntry->presetNumber);
			
			ptz_mutex_unlock();
			return 0;
		}
		
		pEntry=pEntry->next;
	}
	
	ptz_mutex_unlock();
	HTTP_LOG("home positon not found!!!\n");
	return -1;
}

