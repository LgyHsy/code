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
#include "onvif.h"
#include "onvif_device.h"
#include "xml_node.h"
#include "onvif_event.h"
#include "onvif_utils.h"
#include "onvif_cfg.h"
#include "onvif_ptz.h"
#include "util.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include <math.h>
#ifdef LIBICAL
#include "icalvcal.h"
#include "vcc.h"   
#endif
#include "onvif_srv.h"
#include "onvif_probe.h"
#include "../http/http_srv.h"
#include "arp_get_mac.h"
#include "para.h"
#include "anj_mw_net.h"

/***************************************************************************************/

#define HTTP_MAX_CLIENT_NUMS        24

ONVIF_CFG g_onvif_cfg;
ONVIF_CLS g_onvif_cls;
ONVIF_IDX g_onvif_idx;
int thirdstream_enable = 0;
int g_mask_flag[4] = {0};
extern char g_SN[20];
extern char g_uuid[64];
extern char NVR_IP[10][16];				//1

extern int g_ExistWifi;
extern int g_product_type; //型号参数
extern WIFIApConfig g_wifiapCfg;
extern MediaStreamConfig gStreamCfg;
extern MediaConfig	gMediaCfg;
extern OnvifOemStruct g_OemInfo;
extern AjOemStruct g_AJoemInfo;
extern int g_allnet_allocated;				//全网通已被设置标准
extern int g_stereo; //双目
extern int g_onvif_expand;
extern int g_isTTVersion;

#define SERVER_CERT_FILE "./https.crt"
#define SERVER_PRIVATE_KEY_FILE "./https.key"
#define SERVER_CERT_FILE_UPLOAD "/mnt/nand/https.crt"
#define SERVER_PRIVATE_KEY_FILE_UPLOAD "/mnt/nand/https.key"
#define SERVER_PRIVATE_KEY_PWD_UPLOAD "/mnt/nand/https.pwd"

extern int g_hasSMART;//有智能				//1
extern int g_hasSPD;//有人				//1
extern int g_hasSCAR;//有车				//1
extern char searchdomainname[MID_INFO_LENGTH];
extern HttpsPrivateStruct  g_HttpsPrivateInfo; 						//1 https信息

//假如客户定制HTTPS的时候,制造私钥使用了密码
#define SERVER_PRIVATE_KEY_PASS "5MFc62xs67"//"xNwu8iRCt4"

/***************************************************************************************/

char * onvif_get_service_ip(uint32 lip, uint32 rip, char * sip, int len)
{
	*sip = '\0';
	if (g_onvif_cfg.server_ip[0] == '\0')
	{
		if (lip != 0)
		{
			// Receive data from this local IP address and use it
			strncpy(sip, get_ip_str_(lip), len);
		}
		else
		{
			int i;
			int nums = get_if_nums();
			// Find a local address that is on the same network as the remote IP address
			for (i = 0; i < nums; i++)
			{
				uint32 ip = get_if_ip(i);
				uint32 mask = get_if_mask(i);
				if ((ip & mask) == (rip & mask))
				{
					strncpy(sip, get_ip_str_(ip), len);
					break;
				}
			}
		}
	}
	else
	{
		// The server ip address has been configured, use it
		strncpy(sip, g_onvif_cfg.server_ip, len);
	}
	if (sip[0] == '\0')
	{
		strncpy(sip, get_local_ip(), len);
	}
	return sip;
}

char * onvif_get_service_ip_by_user(HTTPCLN * p_user, char * sip, int len)
{
	uint32 lip = 0;
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	if (p_srv->saddr == 0)
	{
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		getsockname(p_user->cfd, (struct sockaddr *)&addr, &addrlen);
		lip = addr.sin_addr.s_addr;
	}
	else
	{
		lip = p_srv->saddr;
	}
	return onvif_get_service_ip(lip, p_user->rip, sip, len);
}

char * onvif_get_service_addr(onvif_CapabilityCategory type, uint32 lip, uint32 rip, char * saddr, int len)
{
	char sip[32] = {'\0'};
	char suffix[64];

	onvif_get_service_ip(lip, rip, sip, sizeof(sip)-1);

	if (CapabilityCategory_Analytics == type)
	{
		strcpy(suffix, "/onvif/analytics_service");
	}
	else if (CapabilityCategory_Device == type)
	{
		strcpy(suffix, "/onvif/device_service");
	}
	else if (CapabilityCategory_Events == type)
	{
		strcpy(suffix, "/onvif/event_service");
	}
	else if (CapabilityCategory_Imaging == type)
	{
		strcpy(suffix, "/onvif/image_service");
	}
	else if (CapabilityCategory_Media == type)
	{
		strcpy(suffix, "/onvif/media");
	}
	else if (CapabilityCategory_PTZ == type)
	{
		strcpy(suffix, "/onvif/ptz_service");
	}
	else if (CapabilityCategory_Recording == type)
	{
		strcpy(suffix, "/onvif/recording_service");
	}
	else if (CapabilityCategory_Search == type)
	{
		strcpy(suffix, "/onvif/search_service");
	}
	else if (CapabilityCategory_Replay == type)
	{
		strcpy(suffix, "/onvif/replay_service");
	}
	else if (CapabilityCategory_AccessControl == type)
	{
		strcpy(suffix, "/onvif/accesscontrol_service");
	}
	else if (CapabilityCategory_DoorControl == type)
	{
		strcpy(suffix, "/onvif/doorcontrol_service");
	}
	else if (CapabilityCategory_DeviceIO == type)
	{
		strcpy(suffix, "/onvif/deviceio_service");
	}
	else if (CapabilityCategory_Media2 == type)
	{
		strcpy(suffix, "/onvif/media2_service");
	}
	else if (CapabilityCategory_Thermal == type)
	{
		strcpy(suffix, "/onvif/thermal_service");
	}
	else if (CapabilityCategory_Credential == type)
	{
		strcpy(suffix, "/onvif/credential_service");
	}
	else if (CapabilityCategory_AccessRules == type)
	{
		strcpy(suffix, "/onvif/accessrules_service");
	}
	else if (CapabilityCategory_Schedule == type)
	{
		strcpy(suffix, "/onvif/schedule_service");
	}
	else if (CapabilityCategory_Receiver == type)
	{
		strcpy(suffix, "/onvif/receiver_service");
	}
	else if (CapabilityCategory_Provisioning == type)
	{
		strcpy(suffix, "/onvif/provisioning_service");
	}
	else if (CapabilityCategory_Hikxsd == type)
	{
		strcpy(suffix, "/onvif/hik_ext");
	}
	else if (CapabilityCategory_Hbgk == type)
	{
		strcpy(suffix, "/onvif/hbgk_ext");
	}
	else if (CapabilityCategory_Telecom == type)
	{
		strcpy(suffix, "/onvif/telecom_service");
	}
	else if (CapabilityCategory_Plus == type)
	{
		strcpy(suffix, "/onvif/Plus");
	}
	else
	{
		strcpy(suffix, "/onvif/device_service");
	}

	if (g_onvif_cfg.http_enable)
	{
		snprintf(saddr, len, "http://%s:%u%s", sip, g_onvif_cls.http_port, suffix);
	}

#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		int offset = strlen(saddr);
		if (offset > 0)
		{
			offset += snprintf(saddr+offset, len-offset, " ");
		}
		snprintf(saddr+offset, len-offset, "https://%s:%u%s", sip, g_onvif_cls.https_port, suffix);
	}
#endif
	log_print(HT_LOG_DBG,  "%s, addr=%s\r\n", __FUNCTION__, saddr);
	return saddr;
}

char * onvif_get_service_addr_by_user(onvif_CapabilityCategory type, HTTPCLN * p_user, char * saddr, int len)
{
	uint32 lip = 0;
	HTTPSRV * p_srv = (HTTPSRV *) p_user->http_srv;
	if (p_srv->saddr == 0)
	{
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		getsockname(p_user->cfd, (struct sockaddr *)&addr, &addrlen);
		lip = addr.sin_addr.s_addr;
	}
	else
	{
		lip = p_srv->saddr;
	}
	return onvif_get_service_addr(type, lip, p_user->rip, saddr, len);
}

HT_API BOOL onvif_is_scope_exist(const char * scope)
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (strcmp(scope, g_onvif_cfg.scopes[i].ScopeItem) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;	
}

HT_API ONVIF_RET onvif_add_scope(const char * scope, BOOL fixed)
{
	onvif_Scope * p_scope;
	if (onvif_is_scope_exist(scope) == TRUE)
	{
		return ONVIF_ERR_ScopeOverwrite;
	}
	p_scope = onvif_get_idle_scope();
	if (p_scope)
	{
		p_scope->ScopeDef = fixed ? ScopeDefinition_Fixed : ScopeDefinition_Configurable;
		strncpy(p_scope->ScopeItem, scope, sizeof(p_scope->ScopeItem)-1);
		return ONVIF_OK;
	}
	return ONVIF_ERR_TooManyScopes;
}

/*
    return 0  :需要替换
    return -1:不允许修改
    return -2:不允许修改
    return -3:不需要修改
*/
HT_API int onvif_compare_nofind_scope(onvif_Scope * p_item, onvif_ScopeParseCnt strScopeCnt)
{
    int nRet = -1;

    if (ScopeDefinition_Fixed == p_item->ScopeDef)
    {
        return nRet;
    }

    if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/Hardware", strlen("onvif://www.onvif.org/Hardware")) == 0)
    {
        if (strScopeCnt.nHardWareCnt != 0)
        {
            nRet = -2;
        }
        else
        {
            nRet = -3;
        }
    }
    else if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/Profile", strlen("onvif://www.onvif.org/Profile")) == 0)
    {
        if (strScopeCnt.nProfileCnt != 0)
        {
            nRet = -2;
        }
        else
        {
            nRet = -3;
        }
    }
    else if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/manufacturer", strlen("onvif://www.onvif.org/manufacturer")) == 0)
    {
        if (strScopeCnt.nManufacCnt != 0)
        {
            nRet = -2;
        }
        else
        {
            nRet = -3;
        }
    }
    else if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/location", strlen("onvif://www.onvif.org/location")) == 0)
    {
        if (strScopeCnt.nLocationCnt != 0)
        {
            nRet = 0;
        }
        else
        {
            nRet = -3;
        }
    }
    else if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/type", strlen("onvif://www.onvif.org/type")) == 0)
    {
        if (strScopeCnt.nTypeCnt != 0)
        {
            nRet = 0;
        }
        else
        {
            nRet = -3;
        }
    }
    else if (strncmp(p_item->ScopeItem, "onvif://www.onvif.org/name", strlen("onvif://www.onvif.org/name")) == 0)
    {
        if (strScopeCnt.nNameCnt != 0)
        {
            nRet = 0;
        }
        else
        {
            nRet = -3;
        }
    }
    else
    {
        if (strScopeCnt.nUserCnt != 0)
        {
            nRet = 0;
        }
        else
        {
            nRet = -3;
        }
    }

	return nRet;
}


HT_API onvif_Scope * onvif_find_scope(const char * scope)
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
        if (g_onvif_cfg.scopes[i].ScopeItem[0] == '\0' )
        {
            continue;
        }
	
		if (strcmp(g_onvif_cfg.scopes[i].ScopeItem, scope) == 0)
		{
			return &g_onvif_cfg.scopes[i];
		}
	}
	return NULL;
}

HT_API onvif_Scope * onvif_get_idle_scope()
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.scopes); i++)
	{
		if (g_onvif_cfg.scopes[i].ScopeItem[0] == '\0')
		{
			return &g_onvif_cfg.scopes[i];
		}
	}
	return NULL;
}

HT_API BOOL onvif_is_user_exist(const char * username)
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] != '\0' && strcmp(username, g_onvif_cfg.users[i].Username) == 0)
		{
			return TRUE;
		}
	}
	return FALSE;	
}

HT_API ONVIF_RET onvif_add_user(onvif_User * p_user)
{
	onvif_User * p_idle_user;
	if (onvif_is_user_exist(p_user->Username) == TRUE)
	{
		return ONVIF_ERR_UsernameClash;
	}
	p_idle_user = onvif_get_idle_user();
	if (p_idle_user)
	{
		memcpy(p_idle_user, p_user, sizeof(onvif_User));
		return ONVIF_OK;
	}
	return ONVIF_ERR_TooManyUsers;
}

HT_API onvif_User * onvif_find_user(const char * username)
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] != '\0' && strcmp(g_onvif_cfg.users[i].Username, username) == 0)
		{
			return &g_onvif_cfg.users[i];
		}
	}
	return NULL;
}

HT_API onvif_User * onvif_get_idle_user()
{
	uint32 i;
	for (i = 0; i < ARRAY_SIZE(g_onvif_cfg.users); i++)
	{
		if (g_onvif_cfg.users[i].Username[0] == '\0')
		{
			return &g_onvif_cfg.users[i];
		}
	}
	return NULL;
}

HT_API const char * onvif_get_user_pass(const char * username)
{
	onvif_User * p_user;
	if (NULL == username || strlen(username) == 0)
	{
		return NULL;
	}
	p_user = onvif_find_user(username);
	if (NULL != p_user)
	{
		return p_user->Password;
	}
	return NULL;	
}

#ifdef GEOLOCATION_SUPPORT

HT_API LocationEntityList * onvif_add_LocationEntity(LocationEntityList ** p_head)
{
	LocationEntityList * p_tmp;
	LocationEntityList * p_new = (LocationEntityList *) malloc(sizeof(LocationEntityList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(LocationEntityList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API LocationEntityList * onvif_find_LocationEntity(LocationEntityList * p_head, const char * Entity, const char * Token)
{
	LocationEntityList * p_tmp = p_head;

	while (p_tmp)
	{
		if (strcmp(p_tmp->Location.Entity, Entity) == 0 && strcmp(p_tmp->Location.Token, Token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_LocationEntity(LocationEntityList ** p_head, LocationEntityList * p_node)
{
	LocationEntityList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_LocationEntitis(LocationEntityList ** p_head)
{
	LocationEntityList * p_next;
	LocationEntityList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API int onvif_get_LocationEntity_nums(LocationEntityList * p_head)
{
	int nums = 0;
	LocationEntityList * p_tmp = p_head;
	while (p_tmp)
	{
		nums++;
		p_tmp = p_tmp->next;
	}
	return nums;
}

#endif // GEOLOCATION_SUPPORT

#ifdef STORAGE_SUPPORT

HT_API void onvif_get_StorageConfiguration_Token(StorageConfigurationList * p_head, char * token, int size)
{
	StorageConfigurationList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "StorageConfigurationToken_%u", ++g_onvif_idx.storage_idx);
		p_tmp = onvif_find_StorageConfiguration(p_head, token);
	}while (p_tmp);
}

HT_API StorageConfigurationList * onvif_add_StorageConfiguration(StorageConfigurationList ** p_head)
{
	StorageConfigurationList * p_tmp;
	StorageConfigurationList * p_new = (StorageConfigurationList *) malloc(sizeof(StorageConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(StorageConfigurationList));
	onvif_get_StorageConfiguration_Token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API StorageConfigurationList * onvif_find_StorageConfiguration(StorageConfigurationList * p_head, const char * token)
{
	StorageConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_StorageConfiguration(StorageConfigurationList ** p_head, StorageConfigurationList * p_node)
{
	StorageConfigurationList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_StorageConfigurations(StorageConfigurationList ** p_head)
{
	StorageConfigurationList * p_next;
	StorageConfigurationList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

#endif

HT_API void onvif_get_profile_token(ONVIF_PROFILE * p_head, char * token, int size)
{
	ONVIF_PROFILE * p_tmp = NULL;
	if (g_stereo)
	{
		char tmp[100] = {0};
		++ g_onvif_idx.profile_idx;
		if (thirdstream_enable)
		{
			if (g_onvif_idx.profile_idx == 1)
				strcpy(tmp, "ProfileToken_1_1");
			else if (g_onvif_idx.profile_idx == 2)
				strcpy(tmp, "ProfileToken_1_2");
			else if (g_onvif_idx.profile_idx == 3)
				strcpy(tmp, "ProfileToken_1_3");
			else if (g_onvif_idx.profile_idx == 4)
				strcpy(tmp, "ProfileToken_2_1");
			else if (g_onvif_idx.profile_idx == 5)
				strcpy(tmp, "ProfileToken_2_2");
			else if (g_onvif_idx.profile_idx == 6)
				strcpy(tmp, "ProfileToken_2_3");
		}
		else
		{
			if (g_onvif_idx.profile_idx == 1)
				strcpy(tmp, "ProfileToken_1_1");
			else if (g_onvif_idx.profile_idx == 2)
				strcpy(tmp, "ProfileToken_1_2");
			else if (g_onvif_idx.profile_idx == 3)
				strcpy(tmp, "ProfileToken_2_1");
			else if (g_onvif_idx.profile_idx == 4)
				strcpy(tmp, "ProfileToken_2_2");
		}
		do {
			snprintf(token, size, tmp);
			p_tmp = onvif_find_profile(p_head, token);
		} while (p_tmp);
	}
	else
	{
		do {
			snprintf(token, size, "ProfileToken_%u", ++g_onvif_idx.profile_idx);

			p_tmp = onvif_find_profile(p_head, token);
		} while (p_tmp);
	}
}

HT_API ONVIF_PROFILE * onvif_add_profile(ONVIF_PROFILE ** p_head, BOOL fixed)
{
	ONVIF_PROFILE * p_tmp;
	ONVIF_PROFILE * p_new = (ONVIF_PROFILE *) malloc(sizeof(ONVIF_PROFILE));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(ONVIF_PROFILE));

	p_new->fixed = fixed;

	onvif_get_profile_token(*p_head, p_new->token, sizeof(p_new->token));
	if (g_stereo)
	{
		char tmp[100] = {0};
		if (thirdstream_enable)
		{
			if (g_onvif_idx.profile_idx == 1)
				strcpy(tmp, "ProfileName_1_1");
			else if (g_onvif_idx.profile_idx == 2)
				strcpy(tmp, "ProfileName_1_2");
			else if (g_onvif_idx.profile_idx == 3)
				strcpy(tmp, "ProfileName_1_3");
			else if (g_onvif_idx.profile_idx == 4)
				strcpy(tmp, "ProfileName_2_1");
			else if (g_onvif_idx.profile_idx == 5)
				strcpy(tmp, "ProfileName_2_2");
			else if (g_onvif_idx.profile_idx == 6)
				strcpy(tmp, "ProfileName_2_3");
		}
		else
		{
			if (g_onvif_idx.profile_idx == 1)
				strcpy(tmp, "ProfileName_1_1");
			else if (g_onvif_idx.profile_idx == 2)
				strcpy(tmp, "ProfileName_1_2");
			else if (g_onvif_idx.profile_idx == 3)
				strcpy(tmp, "ProfileName_2_1");
			else if (g_onvif_idx.profile_idx == 4)
				strcpy(tmp, "ProfileName_2_2");
		}
		sprintf(p_new->name, tmp);
	}
	else
		sprintf(p_new->name, "ProfileName_%u", g_onvif_idx.profile_idx);

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}

	return p_new;
}

HT_API ONVIF_PROFILE * onvif_find_profile(ONVIF_PROFILE * p_head, const char * token)
{
	ONVIF_PROFILE * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_profiles(ONVIF_PROFILE ** p_head)
{
	ONVIF_PROFILE * p_next;
	ONVIF_PROFILE * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
#ifdef PTZ_SUPPORT
		onvif_free_PTZPresets(&p_tmp->presets);
		onvif_free_PresetTours(&p_tmp->preset_tour);
#endif
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoSource_token(VideoSourceList * p_head, char * token, int size)
{
	VideoSourceList * p_tmp = NULL;

	do
	{
		snprintf(token, size, "VideoSourceToken_%u", ++g_onvif_idx.v_src_idx);
		p_tmp = onvif_find_VideoSource(p_head, token);
	}while (p_tmp);
}

HT_API VideoSourceList * onvif_add_VideoSource(VideoSourceList ** p_head, int w, int h, int frame, int max_w, int max_h, int max_frame)
{
	//log_print(HT_LOG_INFO, "w:%d, h:%d, frame:%d, max_w:%d, max_h:%d, max_frame:%d\n", w, h, frame, max_w, max_h, max_frame);
	VideoSourceList * p_tmp;
	VideoSourceList * p_new = (VideoSourceList *) malloc(sizeof(VideoSourceList));
	if (NULL == p_new)
	{
		return NULL;
	}

	memset(p_new, 0, sizeof(VideoSourceList));

	p_new->VideoSource.Framerate = frame;
	p_new->VideoSource.Resolution.Width = w;
	p_new->VideoSource.Resolution.Height = h;

	onvif_get_VideoSource_token(*p_head, p_new->VideoSource.token, sizeof(p_new->VideoSource.token));

	// init video source mode
	p_new->VideoSourceMode.Enabled = 0;
	p_new->VideoSourceMode.Reboot = 0;
	p_new->VideoSourceMode.MaxFramerate = max_frame;
	p_new->VideoSourceMode.MaxResolution.Width = max_w;
	p_new->VideoSourceMode.MaxResolution.Height = max_h;
	sprintf(p_new->VideoSourceMode.token, "VideoSourceModeToken_%u", g_onvif_idx.v_src_idx);
	strcpy(p_new->VideoSourceMode.Encodings, "H264 MJPEG");
#ifdef MPEG4_SUPPORT
	strcat(p_new->VideoSourceMode.Encodings, " MP4");
#endif
#ifdef MEDIA2_SUPPORT
	strcat(p_new->VideoSourceMode.Encodings, " H265");
#endif

#ifdef IMAGE_SUPPORT
	onvif_init_ImagingSettings(p_new, &p_new->ImagingSettings);
	onvif_init_ImagingOptions(p_new, &p_new->ImagingOptions);
#endif

#ifdef THERMAL_SUPPORT
	p_new->ThermalSupport = onvif_init_Thermal(p_new);    
#endif

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoSourceList * onvif_find_VideoSource(VideoSourceList * p_head, const char * token)
{
	VideoSourceList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->VideoSource.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API VideoSourceList * onvif_find_VideoSource_by_size(VideoSourceList * p_head, int w, int h)
{
	VideoSourceList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->VideoSource.Resolution.Width == w && p_tmp->VideoSource.Resolution.Height == h)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_VideoSource(VideoSourceList * p_node)
{
#ifdef THERMAL_SUPPORT
	if (p_node->ThermalConfigurationOptions.ColorPalette)
	{
		onvif_free_ColorPalettes(&p_node->ThermalConfigurationOptions.ColorPalette);
	}
	if (p_node->ThermalConfigurationOptions.NUCTable)
	{
		onvif_free_NUCTables(&p_node->ThermalConfigurationOptions.NUCTable);
	}
#endif    
}

HT_API void onvif_free_VideoSources(VideoSourceList ** p_head)
{
	VideoSourceList * p_next;
	VideoSourceList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_VideoSource(p_tmp);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoSourceConfiguration_token(VideoSourceConfigurationList * p_head, char * token, int size)
{
	VideoSourceConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "VideoSourceConfigurationToken_%u", ++g_onvif_idx.v_src_cfg_idx);
		p_tmp = onvif_find_VideoSourceConfiguration(p_head, token);
	} while (p_tmp);
}


HT_API VideoSourceConfigurationList * onvif_add_VideoSourceConfiguration(VideoSourceConfigurationList ** p_head, int w, int h)
{
	VideoSourceConfigurationList * p_tmp;
	VideoSourceConfigurationList * p_new = (VideoSourceConfigurationList *) malloc(sizeof(VideoSourceConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoSourceConfigurationList));
	p_new->Configuration.Bounds.width = w;
	p_new->Configuration.Bounds.height = h;
	onvif_get_VideoSourceConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "VideoSourceConfigurationName_%d", g_onvif_idx.v_src_cfg_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	onvif_init_VideoSourceConfigurationOptions(p_new);
	return p_new;
}

HT_API VideoSourceConfigurationList * onvif_find_VideoSourceConfiguration(VideoSourceConfigurationList * p_head, const char * token)
{
	VideoSourceConfigurationList * p_tmp = p_head;

	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API VideoSourceConfigurationList * onvif_find_VideoSourceConfiguration_by_size(VideoSourceConfigurationList * p_head, int w, int h)
{
	VideoSourceConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.Bounds.width == w && p_tmp->Configuration.Bounds.height == h)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_VideoSourceConfigurations(VideoSourceConfigurationList ** p_head)
{
	VideoSourceConfigurationList * p_next;
	VideoSourceConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoEncoder2Configuration_token(VideoEncoder2ConfigurationList * p_head, char * token, int size)
{
	VideoEncoder2ConfigurationList * p_tmp = NULL;
	if (g_stereo)
	{
		char tmp[100] = {0};
		++ g_onvif_idx.v_enc_idx;
		if(thirdstream_enable)
		{
			if (g_onvif_idx.v_enc_idx == 1)
				strcpy(tmp, "VideoEncoderConfigurationToken_1_1");
			else if (g_onvif_idx.v_enc_idx == 2)
				strcpy(tmp, "VideoEncoderConfigurationToken_1_2");
			else if (g_onvif_idx.v_enc_idx == 3)
				strcpy(tmp, "VideoEncoderConfigurationToken_1_3");
			else if (g_onvif_idx.v_enc_idx == 4)
				strcpy(tmp, "VideoEncoderConfigurationToken_2_1");
			else if (g_onvif_idx.v_enc_idx == 5)
				strcpy(tmp, "VideoEncoderConfigurationToken_2_2");
			else if (g_onvif_idx.v_enc_idx == 6)
				strcpy(tmp, "VideoEncoderConfigurationToken_2_3");
		}
		else
		{
			if (g_onvif_idx.v_enc_idx == 1)
				strcpy(tmp, "VideoEncoderConfigurationToken_1_1");
			else if (g_onvif_idx.v_enc_idx == 2)
				strcpy(tmp, "VideoEncoderConfigurationToken_1_2");
			else if (g_onvif_idx.v_enc_idx == 3)
				strcpy(tmp, "VideoEncoderConfigurationToken_2_1");
			else if (g_onvif_idx.v_enc_idx == 4)
				strcpy(tmp, "VideoEncoderConfigurationToken_2_2");
		}
		do
		{
			snprintf(token, size, tmp);
			p_tmp = onvif_find_VideoEncoder2Configuration(p_head, token);
		}
		while (p_tmp);
	}
	else
	{
		do
		{
			snprintf(token, size, "VideoEncoderConfigurationToken_%u", ++ g_onvif_idx.v_enc_idx);
			p_tmp = onvif_find_VideoEncoder2Configuration(p_head, token);
		}
		while (p_tmp);
	}
}

HT_API VideoEncoder2ConfigurationList * onvif_add_VideoEncoder2Configuration(VideoEncoder2ConfigurationList ** p_head, VideoEncoder2ConfigurationList * p_node)
{
	VideoEncoder2ConfigurationList * p_tmp;
	VideoEncoder2ConfigurationList * p_new = (VideoEncoder2ConfigurationList *) malloc(sizeof(VideoEncoder2ConfigurationList));
	
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoEncoder2ConfigurationList));
	
	if (p_node)
	{
		memcpy(&p_new->Configuration, &p_node->Configuration, sizeof(onvif_VideoEncoder2Configuration));
	}
	
	onvif_get_VideoEncoder2Configuration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));//获取id-title
	if (g_stereo)
	{
		char tmp[100] = {0};
		if(thirdstream_enable)
		{
			if (g_onvif_idx.v_enc_idx == 1)
				strcpy(tmp, "VideoEncoderConfigurationName_1_1");
			else if (g_onvif_idx.v_enc_idx == 2)
				strcpy(tmp, "VideoEncoderConfigurationName_1_2");
			else if (g_onvif_idx.v_enc_idx == 3)
				strcpy(tmp, "VideoEncoderConfigurationName_1_3");
			else if (g_onvif_idx.v_enc_idx == 4)
				strcpy(tmp, "VideoEncoderConfigurationName_2_1");
			else if (g_onvif_idx.v_enc_idx == 5)
				strcpy(tmp, "VideoEncoderConfigurationName_2_2");
			else if (g_onvif_idx.v_enc_idx == 6)
				strcpy(tmp, "VideoEncoderConfigurationName_2_3");
		}
		else
		{
			if (g_onvif_idx.v_enc_idx == 1)
				strcpy(tmp, "VideoEncoderConfigurationName_1_1");
			else if (g_onvif_idx.v_enc_idx == 2)
				strcpy(tmp, "VideoEncoderConfigurationName_1_2");
			else if (g_onvif_idx.v_enc_idx == 3)
				strcpy(tmp, "VideoEncoderConfigurationName_2_1");
			else if (g_onvif_idx.v_enc_idx == 4)
				strcpy(tmp, "VideoEncoderConfigurationName_2_2");
		}
		sprintf(p_new->Configuration.Name, tmp);
	}
	else
	{
		sprintf(p_new->Configuration.Name, "VideoEncoderConfigurationName_%d", g_onvif_idx.v_enc_idx);
	}
	
	p_new->Configuration.MulticastFlag = 1;
	
	onvif_init_MulticastConfiguration(&p_new->Configuration.Multicast);
	
	onvif_init_VideoEncoderConfigurationOptions(p_new);
	
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoEncoder2ConfigurationList * onvif_find_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_head, const char * token)
{
	VideoEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API VideoEncoder2ConfigurationList * onvif_find_VideoEncoder2Configuration_by_param(VideoEncoder2ConfigurationList * p_head, VideoEncoder2ConfigurationList * p_node)
{
	VideoEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.Resolution.Width == p_node->Configuration.Resolution.Width && 
			p_tmp->Configuration.Resolution.Height == p_node->Configuration.Resolution.Height && 
			fabs(p_tmp->Configuration.Quality - p_node->Configuration.Quality) < 0.1 && 
			p_tmp->Configuration.SessionTimeout == p_node->Configuration.SessionTimeout && 
			fabs(p_tmp->Configuration.RateControl.FrameRateLimit - p_node->Configuration.RateControl.FrameRateLimit) < 0.1 && 
			p_tmp->Configuration.RateControl.EncodingInterval == p_node->Configuration.RateControl.EncodingInterval && 
			p_tmp->Configuration.RateControl.BitrateLimit == p_node->Configuration.RateControl.BitrateLimit && 
			strcmp(p_tmp->Configuration.Encoding, p_node->Configuration.Encoding) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_VideoEncoder2Configurations(VideoEncoder2ConfigurationList ** p_head)
{
	VideoEncoder2ConfigurationList * p_next;
	VideoEncoder2ConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_VideoEncoder2ConfigurationOptions(&p_tmp->Options2);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_NetworkInterface_token(NetworkInterfaceList * p_head, char * token, int size)
{
	NetworkInterfaceList * p_tmp = NULL;
	do {
		snprintf(token, size, "NetworkInterfaceToken_%u", ++g_onvif_idx.netinf_idx);
		p_tmp = onvif_find_NetworkInterface(p_head, token);
	} while (p_tmp);
}

HT_API NetworkInterfaceList * onvif_add_NetworkInterface(NetworkInterfaceList ** p_head)
{
	NetworkInterfaceList * p_tmp;
	NetworkInterfaceList * p_new = (NetworkInterfaceList *) malloc(sizeof(NetworkInterfaceList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(NetworkInterfaceList));
	onvif_get_NetworkInterface_token(*p_head, p_new->NetworkInterface.token, sizeof(p_new->NetworkInterface.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API NetworkInterfaceList * onvif_find_NetworkInterface(NetworkInterfaceList * p_head, const char * token)
{
	NetworkInterfaceList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->NetworkInterface.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_NetworkInterfaces(NetworkInterfaceList ** p_head)
{
	NetworkInterfaceList * p_next;
	NetworkInterfaceList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_OSDConfiguration_token(OSDConfigurationList * p_head, char * token, int size)
{
	OSDConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "OSDConfigurationToken_%u", ++g_onvif_idx.osd_idx);
		p_tmp = onvif_find_OSDConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API OSDConfigurationList * onvif_add_OSDConfiguration(OSDConfigurationList ** p_head)
{
	OSDConfigurationList * p_tmp;
	OSDConfigurationList * p_new = (OSDConfigurationList *) malloc(sizeof(OSDConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(OSDConfigurationList));
	onvif_get_OSDConfiguration_token(*p_head, p_new->OSD.token, sizeof(p_new->OSD.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API OSDConfigurationList * onvif_find_OSDConfiguration(OSDConfigurationList * p_head, const char * token)
{
	OSDConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->OSD.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_OSDConfigurations(OSDConfigurationList ** p_head)
{
	OSDConfigurationList * p_next;
	OSDConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_MetadataConfiguration_token(MetadataConfigurationList * p_head, char * token, int size)
{
	MetadataConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "MetadataConfigurationToken_%u", ++g_onvif_idx.metadata_idx);
		p_tmp = onvif_find_MetadataConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API MetadataConfigurationList * onvif_add_MetadataConfiguration(MetadataConfigurationList ** p_head)
{
	MetadataConfigurationList * p_tmp;
	MetadataConfigurationList * p_new = (MetadataConfigurationList *) malloc(sizeof(MetadataConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(MetadataConfigurationList));
	onvif_get_MetadataConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "MetadataConfigurationName_%u", g_onvif_idx.metadata_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API MetadataConfigurationList * onvif_find_MetadataConfiguration(MetadataConfigurationList * p_head, const char * token)
{
	MetadataConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_MetadataConfigurations(MetadataConfigurationList ** p_head)
{
	MetadataConfigurationList * p_next;
	MetadataConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API VideoEncoder2ConfigurationOptionsList * onvif_add_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head)
{
	VideoEncoder2ConfigurationOptionsList * p_tmp;
	VideoEncoder2ConfigurationOptionsList * p_new = (VideoEncoder2ConfigurationOptionsList *) malloc(sizeof(VideoEncoder2ConfigurationOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoEncoder2ConfigurationOptionsList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoEncoder2ConfigurationOptionsList * onvif_find_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList * p_head, const char * encoding)
{
	VideoEncoder2ConfigurationOptionsList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Options.Encoding, encoding) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head)
{
	VideoEncoder2ConfigurationOptionsList * p_next;
	VideoEncoder2ConfigurationOptionsList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API NotificationMessageList * onvif_add_NotificationMessage(NotificationMessageList ** p_head)
{
	NotificationMessageList * p_tmp;
	NotificationMessageList * p_new = (NotificationMessageList *) malloc(sizeof(NotificationMessageList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(NotificationMessageList));
	if (p_head)
	{
		p_tmp = *p_head;
		if (NULL == p_tmp)
		{
			*p_head = p_new;
		}
		else
		{
			while (p_tmp && p_tmp->next)
				p_tmp = p_tmp->next;
			p_tmp->next = p_new;
		}
	}
	p_new->refcnt++;
	return p_new;
}

HT_API void onvif_free_NotificationMessage(NotificationMessageList * p_message)
{
	if (p_message)
	{
		p_message->refcnt--;
		if (p_message->refcnt <= 0)
		{
			onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Source.SimpleItem);
			onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Key.SimpleItem);
			onvif_free_SimpleItems(&p_message->NotificationMessage.Message.Data.SimpleItem);

			onvif_free_ElementItems(&p_message->NotificationMessage.Message.Source.ElementItem);
			onvif_free_ElementItems(&p_message->NotificationMessage.Message.Key.ElementItem);
			onvif_free_ElementItems(&p_message->NotificationMessage.Message.Data.ElementItem);

            if (p_message->NotificationMessage.AlarmDataLevel != NULL)
            {
                free(p_message->NotificationMessage.AlarmDataLevel);
                p_message->NotificationMessage.AlarmDataLevel = NULL;
            }

			free(p_message);
		}
	}
}

HT_API void onvif_free_NotificationMessages(NotificationMessageList ** p_head)
{
	NotificationMessageList * p_next;
	NotificationMessageList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_NotificationMessage(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API SimpleItemList * onvif_add_SimpleItem(SimpleItemList ** p_head)
{
	SimpleItemList * p_tmp;
	SimpleItemList * p_new = (SimpleItemList *) malloc(sizeof(SimpleItemList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(SimpleItemList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_SimpleItems(SimpleItemList ** p_head)
{
	SimpleItemList * p_next;
	SimpleItemList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API ElementItemList * onvif_add_ElementItem(ElementItemList ** p_head)
{
	ElementItemList * p_tmp;
	ElementItemList * p_new = (ElementItemList *) malloc(sizeof(ElementItemList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ElementItemList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_ElementItems(ElementItemList ** p_head)
{
	ElementItemList * p_next;
	ElementItemList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		if (p_tmp->ElementItem.Any)
		{
			free(p_tmp->ElementItem.Any);
		}
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

#ifdef IMAGE_SUPPORT

HT_API void onvif_get_ImagingPreset_token(ImagingPresetList * p_head, char * token, int size)
{
	ImagingPresetList * p_tmp = NULL;
	do {
		snprintf(token, size, "ImagingPresetToken_%u", ++g_onvif_idx.image_preset_idx);
		p_tmp = onvif_find_ImagingPreset(p_head, token);
	} while (p_tmp);
}

HT_API ImagingPresetList * onvif_add_ImagingPreset(ImagingPresetList ** p_head)
{
	ImagingPresetList * p_tmp;
	ImagingPresetList * p_new = (ImagingPresetList *) malloc(sizeof(ImagingPresetList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ImagingPresetList));
	onvif_get_ImagingPreset_token(*p_head, p_new->Preset.token, sizeof(p_new->Preset.token));
	sprintf(p_new->Preset.Name, "ImagingPresetName_%u", g_onvif_idx.image_preset_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API ImagingPresetList * onvif_find_ImagingPreset(ImagingPresetList * p_head, const char * token)
{
	ImagingPresetList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Preset.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_ImagingPresets(ImagingPresetList ** p_head)
{
	ImagingPresetList * p_next;
	ImagingPresetList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

#endif // IMAGE_SUPPORT

#ifdef AUDIO_SUPPORT

HT_API void onvif_get_AudioSource_token(AudioSourceList * p_head, char * token, int size)
{
	AudioSourceList * p_tmp = NULL;
	do {
		snprintf(token, size, "AudioSourceToken_%u", ++g_onvif_idx.a_src_idx);
		p_tmp = onvif_find_AudioSource(p_head, token);
	} while (p_tmp);
}

HT_API AudioSourceList * onvif_add_AudioSource(AudioSourceList ** p_head)
{
	AudioSourceList * p_tmp;
	AudioSourceList * p_new = (AudioSourceList *) malloc(sizeof(AudioSourceList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioSourceList));
	onvif_get_AudioSource_token(*p_head, p_new->AudioSource.token, sizeof(p_new->AudioSource.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioSourceList * onvif_find_AudioSource(AudioSourceList * p_head, const char * token)
{
	AudioSourceList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->AudioSource.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_AudioSources(AudioSourceList ** p_head)
{
	AudioSourceList * p_next;
	AudioSourceList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_AudioSourceConfiguration_token(AudioSourceConfigurationList * p_head, char * token, int size)
{
	AudioSourceConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "AudioSourceConfigurationToken_%u", ++g_onvif_idx.a_src_cfg_idx);
		p_tmp = onvif_find_AudioSourceConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API AudioSourceConfigurationList * onvif_add_AudioSourceConfiguration(AudioSourceConfigurationList ** p_head)
{
	AudioSourceConfigurationList * p_tmp;
	AudioSourceConfigurationList * p_new = (AudioSourceConfigurationList *) malloc(sizeof(AudioSourceConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioSourceConfigurationList));
	onvif_get_AudioSourceConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "AudioSourceConfigurationName_%u", g_onvif_idx.a_src_cfg_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioSourceConfigurationList * onvif_find_AudioSourceConfiguration(AudioSourceConfigurationList * p_head, const char * token)
{
	AudioSourceConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_AudioSourceConfigurations(AudioSourceConfigurationList ** p_head)
{
	AudioSourceConfigurationList * p_next;
	AudioSourceConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_AudioEncoder2Configuration_token(AudioEncoder2ConfigurationList * p_head, char * token, int size)
{
	AudioEncoder2ConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "AudioEncoderConfigurationToken_%u", ++g_onvif_idx.a_enc_idx);
		p_tmp = onvif_find_AudioEncoder2Configuration(p_head, token);
	} while (p_tmp);
}

HT_API AudioEncoder2ConfigurationList * onvif_add_AudioEncoder2Configuration(AudioEncoder2ConfigurationList ** p_head, AudioEncoder2ConfigurationList * p_node)
{
	AudioEncoder2ConfigurationList * p_tmp;
	AudioEncoder2ConfigurationList * p_new = (AudioEncoder2ConfigurationList *) malloc(sizeof(AudioEncoder2ConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioEncoder2ConfigurationList));
	if (p_node)
	{
		memcpy(&p_new->Configuration, &p_node->Configuration, sizeof(onvif_AudioEncoder2Configuration));
	}
	onvif_get_AudioEncoder2Configuration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "AudioEncoderConfigurationName_%u", g_onvif_idx.a_enc_idx);
	onvif_init_MulticastConfiguration(&p_new->Configuration.Multicast);
	onvif_init_AudioEncoderConfigurationOptions(p_new);

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioEncoder2ConfigurationList * onvif_find_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_head, const char * token)
{
	AudioEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API AudioEncoder2ConfigurationList * onvif_find_AudioEncoder2Configuration_by_param(AudioEncoder2ConfigurationList * p_head, AudioEncoder2ConfigurationList * p_node)
{
	AudioEncoder2ConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Configuration.SessionTimeout == p_node->Configuration.SessionTimeout &&
			p_tmp->Configuration.SampleRate == p_node->Configuration.SampleRate && 
			p_tmp->Configuration.Bitrate == p_node->Configuration.Bitrate && 
			strcmp(p_tmp->Configuration.Encoding, p_node->Configuration.Encoding) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_AudioEncoder2Configurations(AudioEncoder2ConfigurationList ** p_head)
{
	AudioEncoder2ConfigurationList * p_next;
	AudioEncoder2ConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_AudioEncoder2ConfigurationOptions(&p_tmp->Options);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API AudioEncoder2ConfigurationOptionsList * onvif_add_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head)
{
	AudioEncoder2ConfigurationOptionsList * p_tmp;
	AudioEncoder2ConfigurationOptionsList * p_new = (AudioEncoder2ConfigurationOptionsList *) malloc(sizeof(AudioEncoder2ConfigurationOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioEncoder2ConfigurationOptionsList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioEncoder2ConfigurationOptionsList * onvif_find_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList * p_head, const char * encoding)
{
	AudioEncoder2ConfigurationOptionsList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Options.Encoding, encoding) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head)
{
	AudioEncoder2ConfigurationOptionsList * p_next;
	AudioEncoder2ConfigurationOptionsList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_AudioDecoderConfiguration_token(AudioDecoderConfigurationList * p_head, char * token, int size)
{
	AudioDecoderConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "AudioDecoderConfigurationToken_%u", ++g_onvif_idx.a_dec_idx);
		p_tmp = onvif_find_AudioDecoderConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API AudioDecoderConfigurationList * onvif_add_AudioDecoderConfiguration(AudioDecoderConfigurationList ** p_head)
{
	AudioDecoderConfigurationList * p_tmp;
	AudioDecoderConfigurationList * p_new = (AudioDecoderConfigurationList *) malloc(sizeof(AudioDecoderConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioDecoderConfigurationList));
	onvif_get_AudioDecoderConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "AudioDecoderConfigurationName_%u", g_onvif_idx.a_dec_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioDecoderConfigurationList * onvif_find_AudioDecoderConfiguration(AudioDecoderConfigurationList * p_head, const char * token)
{
	AudioDecoderConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_AudioDecoderConfigurations(AudioDecoderConfigurationList ** p_head)
{
	AudioDecoderConfigurationList * p_next;
	AudioDecoderConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
#if defined(MEDIA2_SUPPORT)
		onvif_free_AudioEncoder2ConfigurationOptions(&p_tmp->Options2);
#endif
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

/*
 * Initialize the audio source
 * 
 */

void onvif_init_AudioSource()
{
	AudioSourceList * p_node;
	if (g_onvif_cfg.a_src)
	{
		return;
	}
	int iIndex = 0;
	
	for( iIndex = 0; iIndex < 2 + thirdstream_enable; iIndex ++)
	{
		// todo : here init one audio source (2 channels)
		p_node = onvif_add_AudioSource(&g_onvif_cfg.a_src);
		if (p_node)
		{
			p_node->AudioSource.Channels = 1;
		}
	}
	
}

void onvif_init_AudioSource_old()
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioSource START\n");
	AudioSourceList * p_node;
	if (g_onvif_cfg.a_src)
	{
		return;
	}
	
	// todo : here init one audio source (2 channels)
	p_node = onvif_add_AudioSource(&g_onvif_cfg.a_src);
	if (p_node)
	{
		p_node->AudioSource.Channels = 2;
	}
	//log_print(HT_LOG_INFO, "onvif_init_AudioSource OVER\n");
	return;
}

void onvif_init_AudioSourceConfiguration()
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioSourceConfiguration START\n");
	AudioSourceConfigurationList * p_item;
	if (g_onvif_cfg.a_src_cfg)
	{
		return;
	}
	p_item = onvif_add_AudioSourceConfiguration(&g_onvif_cfg.a_src_cfg);
	if (p_item)
	{
		AudioSourceList * p_a_src = g_onvif_cfg.a_src;
		if (NULL == p_a_src)
		{
			p_a_src = onvif_add_AudioSource(&g_onvif_cfg.a_src);
			if (p_a_src)
			{
				p_a_src->AudioSource.Channels = 1;
			}
		}
		if (p_a_src)
		{
			strcpy(p_item->Configuration.SourceToken, p_a_src->AudioSource.token);
		}
	}
	//log_print(HT_LOG_INFO, "onvif_init_AudioSourceConfiguration OVER\n");
	return;
}

void onvif_init_AudioSourceConfiguration_new()
{
	AudioSourceConfigurationList * p_item;
	if (g_onvif_cfg.a_src_cfg)
	{
		return;
	}
	int iIndex = 0;
	
	for( iIndex = 0; iIndex < 2 + thirdstream_enable; iIndex ++)
	{
		p_item = onvif_add_AudioSourceConfiguration(&g_onvif_cfg.a_src_cfg);
		if (p_item)
		{
			AudioSourceList * p_a_src = g_onvif_cfg.a_src;
			if (NULL == p_a_src)
			{
				p_a_src = onvif_add_AudioSource(&g_onvif_cfg.a_src);
				if (p_a_src)
				{
					p_a_src->AudioSource.Channels = 2;
				}
			}
			if (p_a_src)
			{
				strcpy(p_item->Configuration.SourceToken, p_a_src->AudioSource.token);
			}
		}
	}
	return;
}

void onvif_renew_AudioEncoderConfiguration()
{
	//log_print(HT_LOG_INFO, "onvif_renew_AudioEncoderConfiguration START\n");

	AudioConfig pAudioCfg;
	memset(&pAudioCfg, 0, sizeof(AudioConfig));
	memcpy(&pAudioCfg, &((MediaConfig *)getMediaConfig())->audioConfig, sizeof(pAudioCfg));
	log_print(HT_LOG_INFO, "pAudioCfg.audioEncode.audioEncodeType.typeName:%s\n", pAudioCfg.audioEncode.audioEncodeType.typeName);
	
	AudioEncoder2ConfigurationList *p_a_enc_cfg = g_onvif_cfg.a_enc_cfg;
	if (strstr(pAudioCfg.audioEncode.audioEncodeType.typeName, "G.711"))
		p_a_enc_cfg->Configuration.AudioEncoding = AudioEncoding_G711;
	else
		p_a_enc_cfg->Configuration.AudioEncoding = AudioEncoding_AAC;
	
	p_a_enc_cfg->Configuration.SampleRate = pAudioCfg.audioEncode.sampleRate;
	p_a_enc_cfg->Configuration.Bitrate = pAudioCfg.audioEncode.bitRate;
	
	//log_print(HT_LOG_INFO, "onvif_renew_AudioEncoderConfiguration OVER\n");
	return;
}

void onvif_init_AudioEncoderConfiguration()
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioEncoderConfiguration START\n");
	AudioEncoder2ConfigurationList * p_item;

	if (g_onvif_cfg.a_enc_cfg)
	{
		return;
	}
	
	AudioConfig pAudioCfg;
	memset(&pAudioCfg, 0, sizeof(AudioConfig));
	memcpy(&pAudioCfg, &((MediaConfig *)getMediaConfig())->audioConfig, sizeof(pAudioCfg));
	log_print(HT_LOG_INFO, "pAudioCfg.audioEncode.audioEncodeType.typeName:%s\n", pAudioCfg.audioEncode.audioEncodeType.typeName);
	
	p_item = onvif_add_AudioEncoder2Configuration(&g_onvif_cfg.a_enc_cfg, NULL);
	if (p_item)
	{
		strcpy(p_item->Configuration.Encoding, "PCMU");

		if (strstr(pAudioCfg.audioEncode.audioEncodeType.typeName, "G.711"))
			p_item->Configuration.AudioEncoding = AudioEncoding_G711;
		else
			p_item->Configuration.AudioEncoding = AudioEncoding_AAC;
		
		p_item->Configuration.SampleRate = pAudioCfg.audioEncode.sampleRate;// / 1000;
		p_item->Configuration.Bitrate = pAudioCfg.audioEncode.bitRate;// / 1000;
		p_item->Configuration.SessionTimeout = 10;
	}
	
	p_item->Configuration.MulticastFlag = 1;
	//log_print(HT_LOG_INFO, "onvif_init_AudioEncoderConfiguration OVER\n");
	return;
}

void onvif_init_AudioEncoderConfiguration_new()
{
	AudioEncoder2ConfigurationList * p_item;

	if (g_onvif_cfg.a_enc_cfg)
	{
		return;
	}
	int iIndex = 0;
	
	for( iIndex = 0; iIndex < 2 + thirdstream_enable; iIndex ++)
	{
		p_item = onvif_add_AudioEncoder2Configuration(&g_onvif_cfg.a_enc_cfg, NULL);
		if (p_item)
		{
			strcpy(p_item->Configuration.Encoding, "PCMU");
			p_item->Configuration.AudioEncoding = AudioEncoding_G711;
			p_item->Configuration.SampleRate = 8;
			p_item->Configuration.Bitrate = 64;
			p_item->Configuration.SessionTimeout = 10;
		}
	}
	return;
}

void onvif_init_AudioEncoder2ConfigurationOptions(onvif_AudioEncoder2ConfigurationOptions * p_option, const char * Encoding)
{
	strcpy(p_option->Encoding, Encoding);
	if (strcasecmp(Encoding, "G711") == 0)
	{
		p_option->AudioEncoding = AudioEncoding_G711;
		p_option->BitrateList.sizeItems = 2;
		p_option->BitrateList.Items[0] = 8000;
		p_option->BitrateList.Items[1] = 16000;
		// specify the supported samplerate
		p_option->SampleRateList.sizeItems = 1;
		p_option->SampleRateList.Items[0] = 16000;
	}
	else if (strcasecmp(Encoding, "G711A") == 0)
	{
		p_option->AudioEncoding = AudioEncoding_G711A;
		p_option->BitrateList.sizeItems = 2;
		p_option->BitrateList.Items[0] = 8000;
		p_option->BitrateList.Items[1] = 16000;
		// specify the supported samplerate
		p_option->SampleRateList.sizeItems = 1;
		p_option->SampleRateList.Items[0] = 16000;
	}
	else if (strcasecmp(Encoding, "AAC") == 0)
	{
		p_option->AudioEncoding = AudioEncoding_AAC;
		p_option->BitrateList.sizeItems = 1;
		p_option->BitrateList.Items[0] = 16000;
		// specify the supported samplerate
		p_option->SampleRateList.sizeItems = 1;
		p_option->SampleRateList.Items[0] = 16000;
	}

	
}

/*
 * Initialize the audio encoder configuration options
 * 
 */
HT_API void onvif_init_AudioEncoderConfigurationOptions(AudioEncoder2ConfigurationList * p_item)
{
	AudioEncoder2ConfigurationOptionsList * p_option;

	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_item->Options);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G711");
	
	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_item->Options);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G711A");

	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_item->Options);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "AAC");
	
	//p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_item->Options);

	//onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G726");
}

void onvif_init_AudioDecoderConfigurations()
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioDecoderConfigurations START\n");
	AudioDecoderConfigurationList * p_a_dec_cfg;
#ifdef MEDIA2_SUPPORT
	AudioEncoder2ConfigurationOptionsList * p_option;
#endif

	if (g_onvif_cfg.a_dec_cfg)
	{
		return;
	}

	p_a_dec_cfg = onvif_add_AudioDecoderConfiguration(&g_onvif_cfg.a_dec_cfg);

	p_a_dec_cfg->Options.G711DecOptionsFlag = 1;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.sizeItems = 6;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[0] = 8;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[1] = 12;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[2] = 20;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[3] = 25;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[4] = 32;
	p_a_dec_cfg->Options.G711DecOptions.Bitrate.Items[5] = 40;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.sizeItems = 5;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[0] = 8;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[1] = 12;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[2] = 24;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[3] = 32;
	p_a_dec_cfg->Options.G711DecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G711");
#endif

	p_a_dec_cfg = onvif_add_AudioDecoderConfiguration(&g_onvif_cfg.a_dec_cfg);

	p_a_dec_cfg->Options.G726DecOptionsFlag = 1;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.sizeItems = 6;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[0] = 8;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[1] = 12;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[2] = 20;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[3] = 25;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[4] = 32;
	p_a_dec_cfg->Options.G726DecOptions.Bitrate.Items[5] = 40;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.sizeItems = 5;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[0] = 8;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[1] = 12;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[2] = 24;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[3] = 32;
	p_a_dec_cfg->Options.G726DecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "G711A");
#endif

	p_a_dec_cfg = onvif_add_AudioDecoderConfiguration(&g_onvif_cfg.a_dec_cfg);

	p_a_dec_cfg->Options.AACDecOptionsFlag = 1;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.sizeItems = 6;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[0] = 8;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[1] = 12;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[2] = 20;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[3] = 25;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[4] = 32;
	p_a_dec_cfg->Options.AACDecOptions.Bitrate.Items[5] = 40;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.sizeItems = 5;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[0] = 8;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[1] = 12;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[2] = 24;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[3] = 32;
	p_a_dec_cfg->Options.AACDecOptions.SampleRateRange.Items[4] = 48;

#ifdef MEDIA2_SUPPORT
	p_option = onvif_add_AudioEncoder2ConfigurationOptions(&p_a_dec_cfg->Options2);
	onvif_init_AudioEncoder2ConfigurationOptions(&p_option->Options, "AAC");
#endif
	
	//log_print(HT_LOG_INFO, "onvif_init_AudioDecoderConfigurations OVER\n");
	return;
}

#endif // end of AUDIO_SUPPORT

#ifdef MEDIA2_SUPPORT

HT_API void onvif_get_Mask_token_old(MaskList * p_head, char * token, int size)
{
	MaskList * p_tmp = NULL;
	do {
		snprintf(token, size, "PrivacyMaskToken_%u", ++g_onvif_idx.mask_idx);
		p_tmp = onvif_find_Mask(p_head, token);
	} while (p_tmp);
}

HT_API void onvif_get_Mask_token(MaskList * p_head, char * token, int size, int index)
{
	if (index == -1)
	{
		int i;
		for (i = 0; i < 4; i ++)
		{
			if (g_mask_flag[i] == 0)
			{
				g_mask_flag[i] = 1;
				index = i;
				break;
			}
		}
	}
	snprintf(token, size, "%u", index);
	log_print(HT_LOG_INFO, "onvif_get_Mask_token token index:%d\n", index);
	log_print(HT_LOG_INFO, "onvif_get_Mask_token g_mask_flag[%d, %d, %d, %d]\n", g_mask_flag[0], g_mask_flag[1], g_mask_flag[2], g_mask_flag[3]);
	return ;
}

HT_API void onvif_del_Mask_token(int index)
{
	g_mask_flag[index] = 0;
	log_print(HT_LOG_INFO, "onvif_del_Mask_token g_mask_flag[%d, %d, %d, %d]\n", g_mask_flag[0], g_mask_flag[1], g_mask_flag[2], g_mask_flag[3]);
}

HT_API MaskList * onvif_add_Mask(MaskList ** p_head, int index)
{
	MaskList * p_tmp;
	MaskList * p_new = (MaskList *) malloc(sizeof(MaskList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(MaskList));
	onvif_get_Mask_token(*p_head, p_new->Mask.token, sizeof(p_new->Mask.token), index);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API MaskList * onvif_find_Mask(MaskList * p_head, const char * token)
{
	MaskList * p_tmp = p_head;
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Mask.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Masks(MaskList ** p_head)
{
	MaskList * p_next;
	MaskList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_renew_Masks()//MaskList      * mask)
{
	log_print(HT_LOG_INFO, "onvif_renew_Masks START\n");

	MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
	VideoConfig *pVideoCfg = &pMediaCfg->videoConfig[0];

	int iIndex, imgW[2], imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex ++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);

	float mainX[4]={0},mainY[4]={0},mainW[4]={0},mainH[4]={0}; //,subX[4]={0},subY[4]={0},subW[4]={0},subH[4]={0};
	float p0X[4]={0},p0Y[4]={0},p1X[4]={0},p1Y[4]={0},p2X[4]={0},p2Y[4]={0},p3X[4]={0},p3Y[4]={0};
	int hasMask[4]={0};

	for(iIndex = 0; iIndex < 4; iIndex ++)
	{
		mainX[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].xPos);
		mainY[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].yPos);
		mainW[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].width);
		mainH[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].height);

		if((mainW[iIndex] > 0) && (mainH[iIndex] > 0))
		{
			hasMask[iIndex] = 1;
		}
		
		p0X[iIndex] = mainX[iIndex] / imgW[0] * 2 - 1;
		p0Y[iIndex] = 1 - (mainY[iIndex] / imgH[0] * 2);

		p1X[iIndex] = (mainX[iIndex] + mainW[iIndex]) / imgW[0] * 2 - 1;
		p1Y[iIndex] = 1 - (mainY[iIndex] / imgH[0] * 2);

		p2X[iIndex] = (mainX[iIndex] + mainW[iIndex]) / imgW[0] * 2 - 1;
		p2Y[iIndex] = 1 - ((mainY[iIndex] + mainH[iIndex]) / imgH[0] * 2);

		p3X[iIndex] = mainX[iIndex] / imgW[0] * 2 - 1;
		p3Y[iIndex] = 1 - ((mainY[iIndex] + mainH[iIndex]) / imgH[0] * 2);

		(p0X[iIndex] < -1) ? (p0X[iIndex] = -1) : ((p0X[iIndex] > 1) && (p0X[iIndex] = 1));
		(p1X[iIndex] < -1) ? (p1X[iIndex] = -1) : ((p1X[iIndex] > 1) && (p1X[iIndex] = 1));
		(p2X[iIndex] < -1) ? (p2X[iIndex] = -1) : ((p2X[iIndex] > 1) && (p2X[iIndex] = 1));
		(p3X[iIndex] < -1) ? (p3X[iIndex] = -1) : ((p3X[iIndex] > 1) && (p3X[iIndex] = 1));
		(p0Y[iIndex] < -1) ? (p0Y[iIndex] = -1) : ((p0Y[iIndex] > 1) && (p0Y[iIndex] = 1));
		(p1Y[iIndex] < -1) ? (p1Y[iIndex] = -1) : ((p1Y[iIndex] > 1) && (p1Y[iIndex] = 1));
		(p2Y[iIndex] < -1) ? (p2Y[iIndex] = -1) : ((p2Y[iIndex] > 1) && (p2Y[iIndex] = 1));
		(p3Y[iIndex] < -1) ? (p3Y[iIndex] = -1) : ((p3Y[iIndex] > 1) && (p3Y[iIndex] = 1));
	}
	
	int i = 0;
	
	for (i = 0; i < 4; i++)
	{
		char token[32] = {0};
		sprintf(token, "%d", i);
		MaskList * p_req = onvif_find_Mask(g_onvif_cfg.mask, token);
		if (p_req != NULL && hasMask[i] == 1)
		{
			g_mask_flag[i] = 1;
			p_req->Mask.Enabled = TRUE;
			p_req->Mask.Polygon.sizePoint = 4;
			p_req->Mask.Polygon.Point[0].x = p0X[i];
			p_req->Mask.Polygon.Point[0].y = p0Y[i];
			p_req->Mask.Polygon.Point[1].x = p1X[i];
			p_req->Mask.Polygon.Point[1].y = p1Y[i];
			p_req->Mask.Polygon.Point[2].x = p2X[i];
			p_req->Mask.Polygon.Point[2].y = p2Y[i];
			p_req->Mask.Polygon.Point[3].x = p3X[i];
			p_req->Mask.Polygon.Point[3].y = p3Y[i];
			//log_print(HT_LOG_INFO, "p_req->Mask.token:%s\n", p_req->Mask.token);
			//log_print(HT_LOG_INFO, "Point[0].x:%f, Point[0].y:%f, Point[1].x:%f, Point[1].y:%f, Point[2].x:%f, Point[2].y:%f, Point[3].x:%f, Point[3].y:%f\n", 
			//	p_req->Mask.Polygon.Point[0].x, p_req->Mask.Polygon.Point[0].y, p_req->Mask.Polygon.Point[1].x, p_req->Mask.Polygon.Point[1].y, 
			//	p_req->Mask.Polygon.Point[2].x, p_req->Mask.Polygon.Point[2].y, p_req->Mask.Polygon.Point[3].x, p_req->Mask.Polygon.Point[3].y);
		}
		else if(p_req == NULL && hasMask[i] == 1)
		{
			p_req = onvif_add_Mask(&g_onvif_cfg.mask, i);
			p_req->Mask.Enabled = TRUE;
			p_req->Mask.Polygon.sizePoint = 4;
			p_req->Mask.Polygon.Point[0].x = p0X[i];
			p_req->Mask.Polygon.Point[0].y = p0Y[i];
			p_req->Mask.Polygon.Point[1].x = p1X[i];
			p_req->Mask.Polygon.Point[1].y = p1Y[i];
			p_req->Mask.Polygon.Point[2].x = p2X[i];
			p_req->Mask.Polygon.Point[2].y = p2Y[i];
			p_req->Mask.Polygon.Point[3].x = p3X[i];
			p_req->Mask.Polygon.Point[3].y = p3Y[i];
			strcpy(p_req->Mask.ConfigurationToken, "VideoSourceConfigurationToken_1");
			strcpy(p_req->Mask.Type, "Rectangle");
		}
		else if(p_req != NULL && hasMask[i] == 0)
		{
			onvif_del_Mask_token(i);
			MaskList * p_prev;
			p_prev = g_onvif_cfg.mask;
			if (p_req == p_prev)
			{
				g_onvif_cfg.mask = p_req->next;
			}
			else
			{
				while (p_prev->next)
				{
					if (p_prev->next == p_req)
					{
						break;
					}
					p_prev = p_prev->next;
				}
				p_prev->next = p_req->next;
			}
			free(p_req);
		}
		else
			g_mask_flag[i] = 0;
	}
	
	log_print(HT_LOG_INFO, "onvif_renew_Masks OVER\n");
	return;
}

void onvif_init_Masks()
{
	log_print(HT_LOG_INFO, "onvif_init_Masks START\n");
	MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
	VideoConfig *pVideoCfg = &pMediaCfg->videoConfig[0];

	int iIndex, imgW[2], imgH[2];
	for( iIndex = 0; iIndex < 2; iIndex ++)
	{
		GetVideoSize(pVideoCfg->videoEncode.encodeCfg[iIndex].resolution.name, pVideoCfg->videoCapture.tvsystem, &(imgW[iIndex]), &(imgH[iIndex]));
	}
	VideoMaskConfig *pCfg = &(pVideoCfg->videoMask);

	float mainX[4]={0},mainY[4]={0},mainW[4]={0},mainH[4]={0}; 
	float p0X[4]={0},p0Y[4]={0},p1X[4]={0},p1Y[4]={0},p2X[4]={0},p2Y[4]={0},p3X[4]={0},p3Y[4]={0};
	int hasMask[4]={0};

	for(iIndex = 0; iIndex < 4; iIndex ++)
	{
		mainX[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].xPos);
		mainY[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].yPos);
		mainW[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].width);
		mainH[iIndex] = (float)(pCfg->mainStreamMaskList[iIndex].height);

		if((mainW[iIndex] > 0) && (mainH[iIndex] > 0))
			hasMask[iIndex] = 1;

		p0X[iIndex] = mainX[iIndex] / imgW[0] * 2 - 1;
		p0Y[iIndex] = 1 - (mainY[iIndex] / imgH[0] * 2);

		p1X[iIndex] = (mainX[iIndex] + mainW[iIndex]) / imgW[0] * 2 - 1;
		p1Y[iIndex] = 1 - (mainY[iIndex] / imgH[0] * 2);

		p2X[iIndex] = (mainX[iIndex] + mainW[iIndex]) / imgW[0] * 2 - 1;
		p2Y[iIndex] = 1 - ((mainY[iIndex] + mainH[iIndex]) / imgH[0] * 2);

		p3X[iIndex] = mainX[iIndex] / imgW[0] * 2 - 1;
		p3Y[iIndex] = 1 - ((mainY[iIndex] + mainH[iIndex]) / imgH[0] * 2);

		(p0X[iIndex] < -1) ? (p0X[iIndex] = -1) : ((p0X[iIndex] > 1) && (p0X[iIndex] = 1));
		(p1X[iIndex] < -1) ? (p1X[iIndex] = -1) : ((p1X[iIndex] > 1) && (p1X[iIndex] = 1));
		(p2X[iIndex] < -1) ? (p2X[iIndex] = -1) : ((p2X[iIndex] > 1) && (p2X[iIndex] = 1));
		(p3X[iIndex] < -1) ? (p3X[iIndex] = -1) : ((p3X[iIndex] > 1) && (p3X[iIndex] = 1));
		(p0Y[iIndex] < -1) ? (p0Y[iIndex] = -1) : ((p0Y[iIndex] > 1) && (p0Y[iIndex] = 1));
		(p1Y[iIndex] < -1) ? (p1Y[iIndex] = -1) : ((p1Y[iIndex] > 1) && (p1Y[iIndex] = 1));
		(p2Y[iIndex] < -1) ? (p2Y[iIndex] = -1) : ((p2Y[iIndex] > 1) && (p2Y[iIndex] = 1));
		(p3Y[iIndex] < -1) ? (p3Y[iIndex] = -1) : ((p3Y[iIndex] > 1) && (p3Y[iIndex] = 1));
	}

	int i = 0;
	for (i = 0; i < 4; i++)
	{
		if (hasMask[i] == 1)
		{
			MaskList * p_req = onvif_add_Mask(&g_onvif_cfg.mask, i);
			g_mask_flag[i] = 1;
			p_req->Mask.Enabled = TRUE;
			p_req->Mask.Polygon.sizePoint = 4;
			p_req->Mask.Polygon.Point[0].x = p0X[i];
			p_req->Mask.Polygon.Point[0].y = p0Y[i];
			p_req->Mask.Polygon.Point[1].x = p1X[i];
			p_req->Mask.Polygon.Point[1].y = p1Y[i];
			p_req->Mask.Polygon.Point[2].x = p2X[i];
			p_req->Mask.Polygon.Point[2].y = p2Y[i];
			p_req->Mask.Polygon.Point[3].x = p3X[i];
			p_req->Mask.Polygon.Point[3].y = p3Y[i];
			strcpy(p_req->Mask.ConfigurationToken, "VideoSourceConfigurationToken_1");
			strcpy(p_req->Mask.Type, "Rectangle");
		}
		else
			g_mask_flag[i] = 0;
	}
	
	log_print(HT_LOG_INFO, "onvif_init_Masks OVER\n");
	return;
}

void onvif_init_MaskOptions()
{
	log_print(HT_LOG_INFO, "onvif_init_MaskOptions START\n");
	onvif_MaskOptions * p_opt = &g_onvif_cfg.MaskOptions;

	p_opt->MaxMasks = 4;
	p_opt->MaxPoints = 4;

	p_opt->sizeTypes = 1;
	//strcpy(p_opt->Types[0], "Color");
	strcpy(p_opt->Types[0], "Rectangle");
	//strcpy(p_opt->Types[0], "Pixelated");
	//strcpy(p_opt->Types[2], "Blurred");

	//p_opt->Color.sizeColorList = 1;
	//p_opt->Color.ColorList[0].X = 100;
	//p_opt->Color.ColorList[0].Y = 100;
	//p_opt->Color.ColorList[0].Z = 100;
	//p_opt->Color.ColorList[0].ColorspaceFlag = 1;
	//strcpy(p_opt->Color.ColorList[0].Colorspace, "http://www.onvif.org/ver10/colorspace/YCbCr");
	//p_opt->Color.sizeColorspaceRange = 0;

	p_opt->RectangleOnly = TRUE;
	p_opt->SingleColorOnly = FALSE;
	
	log_print(HT_LOG_INFO, "onvif_init_MaskOptions OVER \n");
	return ;
}

#endif // end of MEDIA2_SUPPORT

#ifdef PTZ_SUPPORT

HT_API void onvif_get_PTZNode_token(PTZNodeList * p_head, char * token, int size)
{
	PTZNodeList * p_tmp = NULL;
	do {
		snprintf(token, size, "PTZNodeToken_%u", ++g_onvif_idx.ptznode_idx);
		p_tmp = onvif_find_PTZNode(p_head, token);
	} while (p_tmp);
}

HT_API PTZNodeList * onvif_add_PTZNode(PTZNodeList ** p_head)
{
	PTZNodeList * p_tmp;
	PTZNodeList * p_new = (PTZNodeList *) malloc(sizeof(PTZNodeList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PTZNodeList));
	onvif_get_PTZNode_token(*p_head, p_new->PTZNode.token, sizeof(p_new->PTZNode.token));
	sprintf(p_new->PTZNode.Name, "PTZNodeName_%u", g_onvif_idx.ptznode_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API PTZNodeList * onvif_find_PTZNode(PTZNodeList * p_head, const char * token)
{
	PTZNodeList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->PTZNode.token, token) == 0)
		{
		    break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_PTZNodes(PTZNodeList ** p_head)
{
	PTZNodeList * p_next;
	PTZNodeList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_PTZConfiguration_token(PTZConfigurationList * p_head, char * token, int size)
{
	PTZConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "PTZConfigurationToken_%u", ++g_onvif_idx.ptzcfg_idx);
		p_tmp = onvif_find_PTZConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API PTZConfigurationList * onvif_add_PTZConfiguration(PTZConfigurationList ** p_head)
{
	PTZConfigurationList * p_tmp;
	PTZConfigurationList * p_new = (PTZConfigurationList *) malloc(sizeof(PTZConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PTZConfigurationList));
	onvif_get_PTZConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "PTZConfigurationName_%u", g_onvif_idx.ptzcfg_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	onvif_init_PTZConfigurationOptions(p_new);
	return p_new;
}

HT_API PTZConfigurationList * onvif_find_PTZConfiguration(PTZConfigurationList * p_head, const char * token)
{
	PTZConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_PTZConfigurations(PTZConfigurationList ** p_head)
{
	PTZConfigurationList * p_next;
	PTZConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_PTZPreset_token(PTZPresetList * p_head, char * token, int size)
{
	PTZPresetList * p_tmp = NULL;
	do {
		snprintf(token, size, "PTZPresetToken_%u", ++g_onvif_idx.preset_idx);
		p_tmp = onvif_find_PTZPreset(p_head, token);
	} while (p_tmp);
}

HT_API PTZPresetList * onvif_add_PTZPreset(PTZPresetList ** p_head)
{
	PTZPresetList * p_tmp;
	PTZPresetList * p_new = (PTZPresetList *) malloc(sizeof(PTZPresetList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PTZPresetList));
	onvif_get_PTZPreset_token(*p_head, p_new->PTZPreset.token, sizeof(p_new->PTZPreset.token));
	sprintf(p_new->PTZPreset.Name, "Preset%03d", g_onvif_idx.preset_idx);

	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API PTZPresetList * onvif_find_PTZPreset(PTZPresetList * p_head, const char  * preset_token)
{
	PTZPresetList * p_tmp = p_head;

	while (p_tmp)
	{

		if (strcmp(p_tmp->PTZPreset.token, preset_token) == 0)
		{
			break;
		}

		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_PTZPreset(PTZPresetList ** p_head, PTZPresetList * p_node)
{
	PTZPresetList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_PTZPresets(PTZPresetList ** p_head)
{
	PTZPresetList * p_next;
	PTZPresetList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API PTZPresetTourSpotList * onvif_add_PTZPresetTourSpot(PTZPresetTourSpotList ** p_head)
{
	PTZPresetTourSpotList * p_tmp;
	PTZPresetTourSpotList * p_new = (PTZPresetTourSpotList *) malloc(sizeof(PTZPresetTourSpotList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PTZPresetTourSpotList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_PTZPresetTourSpots(PTZPresetTourSpotList ** p_head)
{
	PTZPresetTourSpotList * p_next;
	PTZPresetTourSpotList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_PresetTour_token(PresetTourList * p_head, char * token, int size)
{
	PresetTourList * p_tmp = NULL;
	do {
		snprintf(token, size, "PresetTourToken_%u", ++g_onvif_idx.preset_tour_idx);
		p_tmp = onvif_find_PresetTour(p_head, token);
	} while (p_tmp);
}

HT_API PresetTourList * onvif_add_PresetTour(PresetTourList ** p_head)
{
	PresetTourList * p_tmp;
	PresetTourList * p_new = (PresetTourList *) malloc(sizeof(PresetTourList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PresetTourList));
	
	onvif_get_PresetTour_token(*p_head, p_new->PresetTour.token, sizeof(p_new->PresetTour.token));
	sprintf(p_new->PresetTour.Name, "PresetTourName_%u", g_onvif_idx.preset_tour_idx);
	
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API PresetTourList * onvif_find_PresetTour(PresetTourList * p_head, const char * token)
{
	PresetTourList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->PresetTour.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_PresetTour(PresetTourList ** p_head, PresetTourList * p_node)
{
	PresetTourList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	onvif_free_PTZPresetTourSpots(&p_node->PresetTour.TourSpot);
	free(p_node);
}

HT_API void onvif_free_PresetTours(PresetTourList ** p_head)
{
	PresetTourList * p_next;
	PresetTourList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_PTZPresetTourSpots(&p_tmp->PresetTour.TourSpot);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API int onvif_count_PresetTours(PresetTourList * p_head)
{
	int count = 0;
	PresetTourList * p_tmp = p_head;
	while (p_tmp)
	{
		count++;
		p_tmp = p_tmp->next;
	}
	return count;
}

/**
 * init PTZ node
 */
void onvif_init_PTZNode()
{
	//log_print(HT_LOG_INFO, "onvif_init_PTZNode START\n");
	PTZNodeList * p_node;

	if (g_onvif_cfg.ptz_node)
	{
		return;
	}

	// todo : init one ptz node

	p_node = onvif_add_PTZNode(&g_onvif_cfg.ptz_node);
	if (NULL == p_node)
	{
		return;
	}

	p_node->PTZNode.NameFlag = 1;

	p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max = 1.0;
	p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Max = 1.0;
	p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Max = 1.0;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Max = 1.0;

	p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpaceFlag = 1;
	p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpace.XRange.Min = -1.0;
	p_node->PTZNode.SupportedPTZSpaces.ZoomSpeedSpace.XRange.Max = 1.0;

	p_node->PTZNode.MaximumNumberOfPresets = MAX_PTZ_PRESETS;
	p_node->PTZNode.HomeSupported = TRUE;

	p_node->PTZNode.ExtensionFlag = 0;
	p_node->PTZNode.Extension.SupportedPresetTourFlag = 1;
	p_node->PTZNode.Extension.SupportedPresetTour.MaximumNumberOfPresetTours = 8;
	p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Start = 1;
	p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Stop = 1;
	p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Pause = 1;
	p_node->PTZNode.Extension.SupportedPresetTour.PTZPresetTourOperation_Extended = 0;

	p_node->PTZNode.FixedHomePosition = FALSE;
	p_node->PTZNode.GeoMove = TRUE;

	p_node->PTZNode.sizeAuxiliaryCommands = 4;
	strcpy(p_node->PTZNode.AuxiliaryCommands[0], "tt:Wiper|On");
	strcpy(p_node->PTZNode.AuxiliaryCommands[1], "tt:Wiper|Off");
	strcpy(p_node->PTZNode.AuxiliaryCommands[2], "tt:Lamp|On");
	strcpy(p_node->PTZNode.AuxiliaryCommands[3], "tt:Lamp|Off");
	//log_print(HT_LOG_INFO, "onvif_init_PTZNode OVER\n");
	return;
}

/**
 * init ptz configuration
 */
void onvif_init_PTZConfiguration()
{
	//log_print(HT_LOG_INFO, "onvif_init_PTZConfiguration START\n");
	PTZConfigurationList * p_node;

	if (g_onvif_cfg.ptz_cfg)
	{
		return;
	}
	p_node = onvif_add_PTZConfiguration(&g_onvif_cfg.ptz_cfg);
	if (NULL == p_node)
	{
		return;
	}
	if (g_onvif_cfg.ptz_node)
	{
		strcpy(p_node->Configuration.NodeToken, g_onvif_cfg.ptz_node->PTZNode.token);
	}
	else
	{
		log_print(HT_LOG_WARN,  "%s, PTZ node is empty!!!\r\n", __FUNCTION__);
	}

	p_node->Configuration.DefaultPTZSpeedFlag = 1;
	p_node->Configuration.DefaultPTZSpeed.PanTiltFlag = 1;
	p_node->Configuration.DefaultPTZSpeed.PanTilt.x = 1;
	p_node->Configuration.DefaultPTZSpeed.PanTilt.y = 1;
	p_node->Configuration.DefaultPTZSpeed.ZoomFlag = 1;
	p_node->Configuration.DefaultPTZSpeed.Zoom.x = 1;

	p_node->Configuration.DefaultPTZTimeoutFlag = 1;
	p_node->Configuration.DefaultPTZTimeout = 60;

	p_node->Configuration.PanTiltLimitsFlag = 1;
	p_node->Configuration.PanTiltLimits.XRange.Min = -1.0;
	p_node->Configuration.PanTiltLimits.XRange.Max = 1.0;
	p_node->Configuration.PanTiltLimits.YRange.Min = -1.0;
	p_node->Configuration.PanTiltLimits.YRange.Max = 1.0;

	p_node->Configuration.ZoomLimitsFlag = 1;
	p_node->Configuration.ZoomLimits.XRange.Min = -1.0;
	p_node->Configuration.ZoomLimits.XRange.Max = 1.0;

	p_node->Configuration.ExtensionFlag = 1;
	p_node->Configuration.Extension.PTControlDirectionFlag = 1;
	p_node->Configuration.Extension.PTControlDirection.EFlipFlag = 1;
	p_node->Configuration.Extension.PTControlDirection.EFlip = EFlipMode_OFF;
	p_node->Configuration.Extension.PTControlDirection.ReverseFlag = 1;
	p_node->Configuration.Extension.PTControlDirection.Reverse= ReverseMode_OFF;
	
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_PTZConfiguration OVER\n\n");
	return;
}

HT_API void onvif_init_PTZConfigurationOptions(PTZConfigurationList * p_item)
{
	p_item->Options.PTZTimeout.Min = 1;
	p_item->Options.PTZTimeout.Max = 60;

	p_item->Options.PTControlDirectionFlag = 0;
	p_item->Options.PTControlDirection.EFlipMode_OFF = 1;
	p_item->Options.PTControlDirection.EFlipMode_ON = 1;
	p_item->Options.PTControlDirection.ReverseMode_OFF = 1;
	p_item->Options.PTControlDirection.ReverseMode_ON = 1;
	p_item->Options.PTControlDirection.ReverseMode_AUTO = 1;
	return;
}

#endif // end of PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS

HT_API ConfigList * onvif_add_Config(ConfigList ** p_head)
{
	ConfigList * p_tmp;
	ConfigList * p_new = (ConfigList *) malloc(sizeof(ConfigList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ConfigList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_Config(ConfigList * p_node)
{
	onvif_free_SimpleItems(&p_node->Config.Parameters.SimpleItem);
	onvif_free_ElementItems(&p_node->Config.Parameters.ElementItem);
}

HT_API void onvif_free_Configs(ConfigList ** p_head)
{
	ConfigList * p_next;
	ConfigList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_Config(p_tmp);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API ConfigList * onvif_find_Config(ConfigList * p_head, const char * name)
{
	ConfigList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Config.Name, name) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API ConfigList * onvif_find_Config_by_type(ConfigList * p_head, const char * type)
{
	ConfigList * p_tmp = p_head;
	while (p_tmp)
	{
		if (soap_strcmp(p_tmp->Config.Type, type) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_remove_Config(ConfigList ** p_head, ConfigList * p_remove)
{
	BOOL found = FALSE;
	ConfigList * p_prev = NULL;
	ConfigList * p_cfg = *p_head;	
	while (p_cfg)
	{
		if (p_cfg == p_remove)
		{
			found = TRUE;
			break;
		}
		p_prev = p_cfg;
		p_cfg = p_cfg->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_cfg->next;
		}
		else
		{
			p_prev->next = p_cfg->next;
		}
		onvif_free_Config(p_cfg);
		free(p_cfg);
	}
}

HT_API ConfigList * onvif_get_prev_Config(ConfigList * p_head, ConfigList * p_found)
{
	ConfigList * p_prev = p_head;
	if (p_found == p_head)
	{
		return NULL;
	}
	while (p_prev)
	{
		if (p_prev->next == p_found)
		{
			break;
		}
		p_prev = p_prev->next;
	}
	return p_prev;
}

HT_API ConfigDescriptionList * onvif_add_ConfigDescription(ConfigDescriptionList ** p_head)
{
	ConfigDescriptionList * p_tmp;
	ConfigDescriptionList * p_new = (ConfigDescriptionList *) malloc(sizeof(ConfigDescriptionList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ConfigDescriptionList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_ConfigDescriptions(ConfigDescriptionList ** p_head)
{
	ConfigDescriptionList * p_next;
	ConfigDescriptionList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_SimpleItemDescriptions(&p_tmp->ConfigDescription.Parameters.SimpleItemDescription);
		onvif_free_SimpleItemDescriptions(&p_tmp->ConfigDescription.Parameters.ElementItemDescription);

		onvif_free_ConfigDescription_Messages(&p_tmp->ConfigDescription.Messages);

		onvif_free_ConfigOptions(&p_tmp->ConfigOptions);

		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API ConfigDescription_MessagesList * onvif_add_ConfigDescription_Message(ConfigDescription_MessagesList ** p_head)
{
	ConfigDescription_MessagesList * p_tmp;
	ConfigDescription_MessagesList * p_new = (ConfigDescription_MessagesList *) malloc(sizeof(ConfigDescription_MessagesList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ConfigDescription_MessagesList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_ConfigDescription_Message(ConfigDescription_MessagesList * p_item)
{
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Source.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Source.ElementItemDescription);

	onvif_free_SimpleItemDescriptions(&p_item->Messages.Key.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Key.ElementItemDescription);

	onvif_free_SimpleItemDescriptions(&p_item->Messages.Data.SimpleItemDescription);
	onvif_free_SimpleItemDescriptions(&p_item->Messages.Data.ElementItemDescription);
}

HT_API void onvif_free_ConfigDescription_Messages(ConfigDescription_MessagesList ** p_head)
{
	ConfigDescription_MessagesList * p_next;
	ConfigDescription_MessagesList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_ConfigDescription_Message(p_tmp);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API ConfigOptionsList * onvif_add_ConfigOptions(ConfigOptionsList ** p_head)
{
	ConfigOptionsList * p_tmp;
	ConfigOptionsList * p_new = (ConfigOptionsList *) malloc(sizeof(ConfigOptionsList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ConfigOptionsList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_ConfigOptions(ConfigOptionsList ** p_head)
{
	ConfigOptionsList * p_next;
	ConfigOptionsList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		if (p_tmp->Options.any)
		{
			free(p_tmp->Options.any);
		}
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API SimpleItemDescriptionList * onvif_add_SimpleItemDescription(SimpleItemDescriptionList ** p_head)
{
	SimpleItemDescriptionList * p_tmp;
	SimpleItemDescriptionList * p_new = (SimpleItemDescriptionList *) malloc(sizeof(SimpleItemDescriptionList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(SimpleItemDescriptionList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}	
	return p_new;
}

HT_API void onvif_free_SimpleItemDescriptions(SimpleItemDescriptionList ** p_head)
{
	SimpleItemDescriptionList * p_next;
	SimpleItemDescriptionList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoAnalyticsConfiguration_token(VideoAnalyticsConfigurationList * p_head, char * token, int size)
{
	VideoAnalyticsConfigurationList * p_tmp = NULL;
	do {
		snprintf(token, size, "VideoAnalyticsConfigurationToken_%u", ++g_onvif_idx.va_idx);
		p_tmp = onvif_find_VideoAnalyticsConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API VideoAnalyticsConfigurationList * onvif_add_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList ** p_head)
{
	VideoAnalyticsConfigurationList * p_tmp;
	VideoAnalyticsConfigurationList * p_new = (VideoAnalyticsConfigurationList *) malloc(sizeof(VideoAnalyticsConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoAnalyticsConfigurationList));
	onvif_get_VideoAnalyticsConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "VideoAnalyticsConfigurationName_%u", g_onvif_idx.va_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoAnalyticsConfigurationList * onvif_find_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList * p_head, const char * token)
{
	VideoAnalyticsConfigurationList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Configuration.token, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_VideoAnalyticsConfigurations(VideoAnalyticsConfigurationList ** p_head)
{
	VideoAnalyticsConfigurationList * p_next;
	VideoAnalyticsConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_Configs(&p_tmp->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
		onvif_free_Configs(&p_tmp->Configuration.RuleEngineConfiguration.Rule);

		onvif_free_ConfigDescriptions(&p_tmp->SupportedRules.RuleDescription);
		onvif_free_ConfigDescriptions(&p_tmp->SupportedAnalyticsModules.AnalyticsModuleDescription);

		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}
#if 1
void onvif_init_rule_CellMotionDetector(onvif_SupportedRules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:CellMotionDetector");

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "MinCount");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "AlarmOnDelay");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "AlarmOffDelay");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "ActiveCells");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:base64Binary");
		}

		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/CellMotionDetector/Motion");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSourceConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoAnalyticsConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "IsMotion");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "MinCount");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1</tt:Min>"
						"<tt:Max>100</tt:Max>"
					"</tt:IntegerRange>");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "AlarmOnDelay");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1000</tt:Min>"
						"<tt:Max>100000</tt:Max>"
					"</tt:IntegerRange>");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "AlarmOffDelay");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1000</tt:Min>"
						"<tt:Max>100000</tt:Max>"
					"</tt:IntegerRange>");
			}
		}
	}
}

void onvif_init_rule_SmartMotionDetector(onvif_SupportedRules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:SmartMotionDetector");

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Enable");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:boolean");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Sensitivity");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:int");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Threshold");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:duration");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Type");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:int");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Field");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Polygon");
		}
		
		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/FieldDetector/ObjectsInside");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSourceConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoAnalyticsConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xsd:string");
			}

			p_message->Messages.KeyFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Key.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "ObjectId");
				strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
			}
			
			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "IsInside");
				strcpy(p_desc->SimpleItemDescription.Type, "xsd:boolean");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "MinCount");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1</tt:Min>"
						"<tt:Max>100</tt:Max>"
					"</tt:IntegerRange>");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "AlarmOnDelay");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1000</tt:Min>"
						"<tt:Max>100000</tt:Max>"
					"</tt:IntegerRange>");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:CellMotionDetector");
			strcpy(p_options->Options.Name, "AlarmOffDelay");
			strcpy(p_options->Options.Type, "tt:IntegerRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntegerRange>"
						"<tt:Min>1000</tt:Min>"
						"<tt:Max>100000</tt:Max>"
					"</tt:IntegerRange>");
			}
		}
	}
}

void onvif_init_rule_MotionRegionDetector(onvif_SupportedRules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:MotionRegionDetector");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 10;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "MotionRegion");
			strcpy(p_desc->SimpleItemDescription.Type, "axt:MotionRegionConfig");
		}

		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/MotionRegionDetector/Motion");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "RuleName");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "State");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			strcpy(p_options->Options.RuleType, "tt:MotionRegionDetector");
			strcpy(p_options->Options.Name, "MotionRegion");
			strcpy(p_options->Options.Type, "axt:MotionRegionConfigOptions");

			p_options->Options.any = (char *) malloc(1024);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<axt:MotionRegionConfigOptions>\r\n"
						"<DisarmSupport>true</DisarmSupport>\r\n"
						"<PolygonSupport>true</PolygonSupport>\r\n"
						"<PolygonLimits>\r\n"
							"<tt:Min>1</tt:Min>\r\n"
							"<tt:Max>20</tt:Max>\r\n"
						"</PolygonLimits>\r\n"
						"<RuleNotification>true</RuleNotification>\r\n"
						"<SingleSensitivitySupport>true</SingleSensitivitySupport>\r\n"
						"<PTZPresetMotionSupport>true</PTZPresetMotionSupport>\r\n"
					"</axt:MotionRegionConfigOptions>\r\n");
			}
		}
	}
}

void onvif_init_rule_FaceRecognition(onvif_SupportedRules * p_item)
{
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:FaceRecognition");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "IncludeImage");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
		}

		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/Recognition/Face");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "AnalyticsConfiguration");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Likelihood");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:float");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Label");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "ImageUri");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:anyURI");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "EnrollmentID");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "RefImageUri");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:anyURI");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Image");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:base64Binary");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "BoundingBox");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:Rectangle");
			}
		}
	}
}

void onvif_init_rule_LicensePlateRecognition(onvif_SupportedRules * p_item)
{
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:LicensePlateRecognition");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "IncludeImage");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "PlateLocation");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Region");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Polygon");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "SnapLine");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Polyline");
		}
		
		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/Recognition/LicensePlate");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "AnalyticsConfiguration");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Likelihood");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:float");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Label");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "ImageUri");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:anyURI");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VehicleImageURI");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:anyURI");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "BoundingBox");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:Rectangle");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Image");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:base64Binary");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "LicensePlateInfo");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:LicensePlateInfo");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VehicleInfo");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:VehicleInfo");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.ElementItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VehicleImage");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:base64Binary");
			}
		}
	}
}

void onvif_init_rule_LineCounting(onvif_SupportedRules * p_item)
{
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:LineCounting");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "ReportTimeInterval");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:duration");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "ResetTime");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:time");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Direction");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Direction");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "PassAllPolylines");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Segments");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Polyline");
		}

		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/CountAggregation/Counter");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "AnalyticsConfiguration");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Count");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:int");
			}
		}
	}
}

void onvif_init_rule_TamperingDetection(onvif_SupportedRules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->RuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:TamperingDetection");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Mode");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Threshold");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:float");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Duration");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:duration");
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:TamperingDetection");
			strcpy(p_options->Options.Name, "Mode");
			strcpy(p_options->Options.Type, "tt:StringItems");

			p_options->Options.any = (char *) malloc(500);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:StringItems>\r\n"
						"<tt:Item>GlobalSceneChange</tt:Item>\r\n"
						"<tt:Item>ImageTooDark</tt:Item>\r\n"
						"<tt:Item>ImageTooBright</tt:Item>\r\n"
						"<tt:Item>SignalLoss</tt:Item>\r\n"
						"<tt:Item>ImageTooBlurry</tt:Item>\r\n"
					"</tt:StringItems>\r\n");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:TamperingDetection");
			strcpy(p_options->Options.Name, "Threshold");
			strcpy(p_options->Options.Type, "tt:FloatRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:FloatRange>\r\n"
						"<tt:Min>0.0</tt:Min>\r\n"
						"<tt:Max>100.0</tt:Max>\r\n"
					"</tt:FloatRange>\r\n");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			p_options->Options.RuleTypeFlag = 1;
			strcpy(p_options->Options.RuleType, "tt:TamperingDetection");
			strcpy(p_options->Options.Name, "Duration");
			strcpy(p_options->Options.Type, "tt:DurationRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:DurationRange>\r\n"
						"<tt:Min>PT1S</tt:Min>\r\n"
						"<tt:Max>PT1M</tt:Max>\r\n"
					"</tt:DurationRange>\r\n");
			}
		}
	}
}

void onvif_init_SupportedRules(onvif_SupportedRules * p_item)
{
	//log_print(HT_LOG_INFO, "onvif_init_SupportedRules START\n");
	p_item->sizeRuleContentSchemaLocation = 1;
	strcpy(p_item->RuleContentSchemaLocation[0], "http://www.w3.org/2001/XMLSchema");

	// add tt:CellMotionDetector rule
	onvif_init_rule_CellMotionDetector(p_item);

	// add tt:SmartMotionDetector rule
	if (g_hasSPD || g_hasSCAR || g_hasSMART)
	{
		onvif_init_rule_SmartMotionDetector(p_item);
	}
	// add tt:MotionRegionDetector rule
	//onvif_init_rule_MotionRegionDetector(p_item);

	// add tt:FaceRecognition rule
	//onvif_init_rule_FaceRecognition(p_item);

	// add tt:LicensePlateRecognition rule
	//onvif_init_rule_LicensePlateRecognition(p_item);

	// add tt:LineCounting rule
	//onvif_init_rule_LineCounting(p_item);

	// add tt:TamperingDetection
	//onvif_init_rule_TamperingDetection(p_item);
	//log_print(HT_LOG_INFO, "onvif_init_SupportedRules OVER\n");
	return;
}
#endif
#if 1

void onvif_init_SmartMotionDetectorEngine(onvif_SupportedAnalyticsModules * p_item)
{
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->AnalyticsModuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:SmartMotionDetectorEngine");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Enable");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:boolean");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Layout");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Transformation");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Field");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:PolygonConfiguration");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Date");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:Week");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Type");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
		}
	}
}

void onvif_init_CellMotionDetector(onvif_SupportedAnalyticsModules * p_item)
{
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->AnalyticsModuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:CellMotionDetector");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "MinCount");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "AlarmOnDelay");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "AlarmOffDelay");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "ActiveCells");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:base64Binary");
		}
		
	}
}

void onvif_init_CellMotionEngine(onvif_SupportedAnalyticsModules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	//ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->AnalyticsModuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:CellMotionEngine");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Sensitivity");
			strcpy(p_desc->SimpleItemDescription.Type, "xsd:integer");
		}

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.ElementItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Layout");
			strcpy(p_desc->SimpleItemDescription.Type, "tt:CellLayout");
		}

		/*p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/CellMotionDetector/Motion");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSourceConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoAnalyticsConfigurationToken");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "Rule");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "IsMotion");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
			}
		}*/

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			strcpy(p_options->Options.RuleType, "tt:CellMotionEngine");
			strcpy(p_options->Options.Name, "Sensitivity");
			strcpy(p_options->Options.Type, "tt:IntRange");

			p_options->Options.any = (char *) malloc(128);
			if (p_options->Options.any)
			{
				strcpy(p_options->Options.any, 
					"<tt:IntRange>\r\n"
						"<tt:Min>0</tt:Min>\r\n"
						"<tt:Max>100</tt:Max>\r\n"
					"</tt:IntRange>\r\n");
			}
		}
	}
}

void onvif_init_MotionRegionDetector(onvif_SupportedAnalyticsModules * p_item)
{
	ConfigOptionsList * p_options;
	ConfigDescriptionList * p_cfg_desc;
	SimpleItemDescriptionList * p_desc;
	ConfigDescription_MessagesList * p_message;

	p_cfg_desc = onvif_add_ConfigDescription(&p_item->AnalyticsModuleDescription);
	if (p_cfg_desc)
	{
		strcpy(p_cfg_desc->ConfigDescription.Name, "tt:MotionRegionDetector");
		p_cfg_desc->ConfigDescription.maxInstancesFlag = 1;
		p_cfg_desc->ConfigDescription.maxInstances = 1;

		p_desc = onvif_add_SimpleItemDescription(&p_cfg_desc->ConfigDescription.Parameters.SimpleItemDescription);
		if (p_desc)
		{
			strcpy(p_desc->SimpleItemDescription.Name, "Sensitivity");
			strcpy(p_desc->SimpleItemDescription.Type, "xs:integer");
		}

		p_message = onvif_add_ConfigDescription_Message(&p_cfg_desc->ConfigDescription.Messages);
		if (p_message)
		{
			p_message->Messages.IsPropertyFlag = 1;
			p_message->Messages.IsProperty = TRUE;
			strcpy(p_message->Messages.ParentTopic, "tns1:RuleEngine/MotionRegionDetector/Motion");

			p_message->Messages.SourceFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "VideoSource");
				strcpy(p_desc->SimpleItemDescription.Type, "tt:ReferenceToken");
			}

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Source.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "RuleName");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:string");
			}

			p_message->Messages.DataFlag = 1;

			p_desc = onvif_add_SimpleItemDescription(&p_message->Messages.Data.SimpleItemDescription);
			if (p_desc)
			{
				strcpy(p_desc->SimpleItemDescription.Name, "State");
				strcpy(p_desc->SimpleItemDescription.Type, "xs:boolean");
			}
		}

		p_options = onvif_add_ConfigOptions(&p_cfg_desc->ConfigOptions);
		if (p_options)
		{
			strcpy(p_options->Options.RuleType, "tt:MotionRegionDetector");
			strcpy(p_options->Options.Name, "Sensitivity");
			strcpy(p_options->Options.Type, "tt:IntRange");

			p_options->Options.any = (char *) malloc(128);

			strcpy(p_options->Options.any, 
				"<tt:IntRange>\r\n"
					"<tt:Min>0</tt:Min>\r\n"
					"<tt:Max>10</tt:Max>\r\n"
				"</tt:IntRange>\r\n");
		}
	}
}

void onvif_init_SupportedAnalyticsModules(onvif_SupportedAnalyticsModules * p_item)
{
	//log_print(HT_LOG_INFO, "onvif_init_SupportedAnalyticsModules START\n");
	p_item->sizeAnalyticsModuleContentSchemaLocation = 1;
	strcpy(p_item->AnalyticsModuleContentSchemaLocation[0], "http://www.w3.org/2001/XMLSchema");

	// add tt:CellMotionEngine AnalyticsModule
	onvif_init_CellMotionEngine(p_item);

	// add tt:CellMotionDetector AnalyticsModule
	onvif_init_CellMotionDetector(p_item);

	// add tt:SmartMotionDetectorEngine AnalyticsModule
	onvif_init_SmartMotionDetectorEngine(p_item);
	
	// add tt:MotionRegionDetector AnalyticsModule
	//onvif_init_MotionRegionDetector(p_item);
	//log_print(HT_LOG_INFO, "onvif_init_SupportedAnalyticsModules OVER\n");
	return;
}

#endif

void onvif_renew_VideoAnalyticsConfiguration()
{
	log_print(HT_LOG_INFO, "onvif_renew_VideoAnalyticsConfiguration START\n");
	ConfigList * p_config;
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	VideoAnalyticsConfigurationList * p_va_cfg;
	
	
	p_va_cfg = onvif_find_VideoAnalyticsConfiguration(g_onvif_cfg.va_cfg, "VideoAnalyticsConfigurationToken_1");
	if (NULL == p_va_cfg)
	{
		log_print(HT_LOG_INFO, "onvif_find_VideoAnalyticsConfiguration\n");
		return;
	}

	MotionDetectAlarm md_alarm;
	memset(&md_alarm, 0, sizeof(MotionDetectAlarm));
	memcpy(&md_alarm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm));
	
	if (1)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, "MyCellMotionModule");
		if (p_config)
		{
			onvif_free_Config(p_config);
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
				sprintf(p_simpleitem->SimpleItem.Value, "%d", md_alarm.sensitivity);
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Layout");
				p_elementitem->ElementItem.Any = (char *)malloc(1024);
				if (p_elementitem->ElementItem.Any != NULL)
				{
					p_elementitem->ElementItem.AnyFlag = 1;
					
					char buf[1024];
					int nBlockX = (md_alarm.blockCount & 0xffff0000) >> 16;
					int nBlockY = md_alarm.blockCount & 0x0000ffff;
					sprintf(buf, "<tt:CellLayout Rows=\"%d\" Columns=\"%d\">"
							"<tt:Transformation>"
							"<tt:Translate y=\"-1\" x=\"-1\" />" 
							"<tt:Scale y=\"9.99999997E-07\" x=\"9.99999997E-07\" />" 
							"</tt:Transformation>"
							"</tt:CellLayout>", nBlockY, nBlockX);
					strcpy(p_elementitem->ElementItem.Any, buf);
				}
			}
		}
	}

	if (1)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule, "MyMotionDetectorRule");
		if (p_config)
		{
			onvif_free_Config(p_config);
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "MinCount");
				strcpy(p_simpleitem->SimpleItem.Value, "5");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOnDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOffDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "ActiveCells");
				strcpy(p_simpleitem->SimpleItem.Value, "zwA=");
				if (1)
				{
					char *value = NULL;
					Aj_Get_ActiveCells(&md_alarm, &value);
					
					if (value != NULL)
					{
						memset(p_simpleitem->SimpleItem.Value, 0, ONVIF_TOKEN_LEN);
						strcpy(p_simpleitem->SimpleItem.Value, value);
						free(value);
					}
				}
			}
		}
	}
	
	if (1)
	{
		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, "MyMotionDetectorRule");
		if (p_config)
		{
			onvif_free_Config(p_config);
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "MinCount");
				strcpy(p_simpleitem->SimpleItem.Value, "5");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOnDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOffDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "ActiveCells");
				strcpy(p_simpleitem->SimpleItem.Value, "zwA=");
				
				char *value = NULL;
				Aj_Get_ActiveCells(&md_alarm, &value);
				if (value != NULL)
				{
					memset(p_simpleitem->SimpleItem.Value, 0, ONVIF_TOKEN_LEN);
					strcpy(p_simpleitem->SimpleItem.Value, value);
					free(value);
				}
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "hb_ext");
				
				p_elementitem->ElementItem.AnyFlag = 1;
				p_elementitem->ElementItem.Any = (char *)malloc(10240);
				
				int MDA_enable = md_alarm.enable;
				if(MDA_enable == 0)
				{
					sprintf(p_elementitem->ElementItem.Any, "<tt:SimpleItem Name=\"MotionDetectorEnable\" Value=\"false\" />");
				}
				else
				{
					sprintf(p_elementitem->ElementItem.Any, "<tt:SimpleItem Name=\"MotionDetectorEnable\" Value=\"true\" />");
					TimeSpanList timeSpanList;
					TransTimeSpan2Old(&md_alarm.timeSpan, &timeSpanList);
					
					int MDA_WDCnt = timeSpanList.workdayCnt;
					int i = 0;
					for(i = 0; i < MDA_WDCnt; i ++)
					{
						switch(timeSpanList.workdayTimes[i].workday)
						{
							case 0:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Sunday\">");
								break;
							case 1:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Monday\">");
								break;
							case 2:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Tuesday\">");
								break;
							case 3:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Wednesday\">");
								break;
							case 4:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Thursday\">");
								break;
							case 5:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Friday\">");
								break;
							case 6:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Saturday\">");
								break;
							case 7:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
								break;
							default:
								strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
								break;
						}
						
						int MDA_TSCnt = timeSpanList.workdayTimes[i].timeSpancnt;
						int tIndex = 0;
						for(tIndex = 0; tIndex < MDA_TSCnt; tIndex ++)
						{
							int startH = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.hour;
							int startM = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.minute;
							int startS = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.sec;
							int endH = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.hour;
							int endM = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.minute;
							int endS = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.sec;
							char timeBUF[100] = {0};
							sprintf(timeBUF, "<tt:SimpleItem Name=\"time_seg\" Value=\"%02d:%02d:%02d-%02d:%02d:%02d\" />", startH, startM, startS, endH, endM, endS);
							strcat(p_elementitem->ElementItem.Any, timeBUF);
						}
						strcat(p_elementitem->ElementItem.Any, "</tt:ElementItem>");
						
					}
				}
			}
		}
	}

	if (1)
	{
		PdAlarm pAlarmt;
		memset(&pAlarmt, 0, sizeof(PdAlarm));
		AlarmConfig *pTmpAlarmConfig = (AlarmConfig *)getAlarmConfig();
		memcpy(&pAlarmt, &pTmpAlarmConfig->aiAlarm.pdAlarm[0], sizeof(PdAlarm));
		
		p_config = onvif_find_Config(p_va_cfg->Configuration.RuleEngineConfiguration.Rule, "MySmartMotionDetector");
		if (p_config)
		{
			onvif_free_Config(p_config);
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Enable");
				if (pAlarmt.enable == 0)
					strcpy(p_simpleitem->SimpleItem.Value, "false");
				else
					strcpy(p_simpleitem->SimpleItem.Value, "true");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
				char Sensitivity_msg[64] = {0};
				sprintf(Sensitivity_msg, "%d", pAlarmt.sensitivity*10 + pAlarmt.threshold);
				strcpy(p_simpleitem->SimpleItem.Value, Sensitivity_msg);
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Reserved");
				strcpy(p_simpleitem->SimpleItem.Value, "0");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Type");
				char type_buf[33] = {0};
				type_buf[32] = '\0';
				ToBin(pAlarmt.type, type_buf);
				if ((type_buf[31-0] == 0x31) && (type_buf[31-4] == 0x31))
					strcpy(p_simpleitem->SimpleItem.Value, "3");
				else if ((type_buf[31-0] == 0x31) && (type_buf[31-4] == 0x30))
					strcpy(p_simpleitem->SimpleItem.Value, "2");
				else if ((type_buf[31-0] == 0x30) && (type_buf[31-4] == 0x31))
					strcpy(p_simpleitem->SimpleItem.Value, "1");
				else if ((type_buf[31-0] == 0x30) && (type_buf[31-4] == 0x30))
					strcpy(p_simpleitem->SimpleItem.Value, "0");
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Field");
				p_elementitem->ElementItem.AnyFlag = 1;

				char Field_info[512] = {0};
				char Field_msg1[512] = "<tt:PolygonConfiguration>\n\
                            <tt:Polygon>\n";
				char Field_msg2[128] =  "<tt:Point x=\"%d\" y=\"%d\"/>\n";
				char Field_msg3[128] =  "</tt:Polygon>\n\
                       </tt:PolygonConfiguration>";
				char Field_msg4[128] = {0};
				
				int i = 0;
				for (i = 0; i < pAlarmt.polygonArea.count; i ++)
				{
					sprintf(Field_msg4, Field_msg2, (pAlarmt.polygonArea.points[i].x)*100, (pAlarmt.polygonArea.points[i].y)*100);
					strcat(Field_msg1, Field_msg4);
					memset(Field_msg4, 0, 128);
				}
				
				strcat(Field_msg1, Field_msg3);
				strcat(Field_info, Field_msg1);
				
				p_elementitem->ElementItem.Any = (char *)malloc(strlen(Field_info));
				strcpy(p_elementitem->ElementItem.Any, Field_info);
				
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Date");
				p_elementitem->ElementItem.AnyFlag = 1;
				
				TimeSpanList timeSpanList1;
				memset(&timeSpanList1, 0, sizeof(TimeSpanList));
				TransTimeSpan2Old(&(pAlarmt.timeSpan), &timeSpanList1);
				char my_pAnyBuf[1024 * 6] = {0};
				int MDA_WDCnt1 = timeSpanList1.workdayCnt;
				
				int i = 0;
				for(i = 0; i < MDA_WDCnt1; i ++)
				{
					switch(timeSpanList1.workdayTimes[i].workday)
					{
						case 0:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Sunday\">");
							break;
						case 1:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Monday\">");
							break;
						case 2:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Tuesday\">");
							break;
						case 3:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Wednesday\">");
							break;
						case 4:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Thursday\">");
							break;
						case 5:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Friday\">");
							break;
						case 6:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Saturday\">");
							break;
						case 7:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
							break;
						default:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
							break;
					}
					
					int MDA_TSCnt = timeSpanList1.workdayTimes[i].timeSpancnt;
					int tIndex = 0;
					for(tIndex = 0; tIndex < MDA_TSCnt; tIndex ++)
					{
						int startH = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.hour;
						int startM = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.minute;
						int startS = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.sec;
						int endH = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.hour;
						int endM = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.minute;
						int endS = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.sec;
						char timeBUF[100] = {0};
						sprintf(timeBUF, "<tt:SimpleItem Name=\"time_seg\" Value=\"%02d:%02d:%02d-%02d:%02d:%02d\" />",
							startH, startM, startS, endH, endM, endS);
						strcat(my_pAnyBuf, timeBUF);
					}
					strcat(my_pAnyBuf, "</tt:ElementItem>");
				}
				
				p_elementitem->ElementItem.Any = (char *)malloc(strlen(my_pAnyBuf) + 1);
				strcpy(p_elementitem->ElementItem.Any, my_pAnyBuf);
			}
		}
	}
	log_print(HT_LOG_INFO, "onvif_renew_VideoAnalyticsConfiguration OVER\n");
	return;
}
void onvif_init_VideoAnalyticsConfiguration()
{
	//log_print(HT_LOG_INFO, "onvif_init_VideoAnalyticsConfiguration START\n");
	// todo : here init video analytics configurations ...
	ConfigList * p_config;
	SimpleItemList * p_simpleitem;
	ElementItemList * p_elementitem;
	VideoAnalyticsConfigurationList * p_va_cfg;
	if (g_onvif_cfg.va_cfg)
	{
		return;
	}
	
	p_va_cfg = onvif_add_VideoAnalyticsConfiguration(&g_onvif_cfg.va_cfg);
	if (NULL == p_va_cfg)
	{
		return;
	}
	
	// todo : here init analytics engine configuration ...
	MotionDetectAlarm md_alarm;
	memset(&md_alarm, 0, sizeof(MotionDetectAlarm));
	memcpy(&md_alarm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.motionDetectAlarm[0], sizeof(md_alarm));

	PdAlarm pAlarmt ;
	memset(&pAlarmt, 0, sizeof(PdAlarm));
	AlarmConfig *pTmpAlarmConfig = (AlarmConfig *)getAlarmConfig();
	memcpy(&pAlarmt, &pTmpAlarmConfig->aiAlarm.pdAlarm[0], sizeof(PdAlarm));
	if (1)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
		if (p_config)
		{
			memset(p_config->Config.Name, 0, 100);
			memset(p_config->Config.Type, 0, 100);
			strcpy(p_config->Config.Name, "MyCellMotionModule");
			strcpy(p_config->Config.Type, "tt:CellMotionEngine");

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
				sprintf(p_simpleitem->SimpleItem.Value, "%d", md_alarm.sensitivity);
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Layout");
				p_elementitem->ElementItem.Any = (char *)malloc(1024);
				if (p_elementitem->ElementItem.Any != NULL)
				{
					p_elementitem->ElementItem.AnyFlag = 1;
					
					char buf[1024];
					int nBlockX = (md_alarm.blockCount & 0xffff0000) >> 16;
					int nBlockY = md_alarm.blockCount & 0x0000ffff;
					sprintf(buf, "<tt:CellLayout Rows=\"%d\" Columns=\"%d\">"
							"<tt:Transformation>"
							"<tt:Translate y=\"-1\" x=\"-1\" />" 
							"<tt:Scale y=\"9.99999997E-07\" x=\"9.99999997E-07\" />" 
							"</tt:Transformation>"
							"</tt:CellLayout>", nBlockY, nBlockX);
					strcpy(p_elementitem->ElementItem.Any, buf);
				}
			}
		}
	}

	if (1)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
		if (p_config)
		{
			memset(p_config->Config.Name, 0, 100);
			memset(p_config->Config.Type, 0, 100);
			strcpy(p_config->Config.Name, "MyMotionDetectorRule");
			strcpy(p_config->Config.Type, "tt:CellMotionDetector");
			
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "MinCount");
				strcpy(p_simpleitem->SimpleItem.Value, "5");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOnDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOffDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "ActiveCells");
				strcpy(p_simpleitem->SimpleItem.Value, "zwA=");
				if (1)
				{
					char *value = NULL;
					Aj_Get_ActiveCells(&md_alarm, &value);
					
					if (value != NULL)
					{
						memset(p_simpleitem->SimpleItem.Value, 0, ONVIF_TOKEN_LEN);
						strcpy(p_simpleitem->SimpleItem.Value, value);
						free(value);
					}
				}
			}
		}
	}
	
	if (g_hasSPD || g_hasSCAR || g_hasSMART)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
		if (p_config)
		{
			memset(p_config->Config.Name, 0, 100);
			memset(p_config->Config.Type, 0, 100);
			strcpy(p_config->Config.Name, "MySmartMotionDetector");
			strcpy(p_config->Config.Type, "tt:SmartMotionDetectorEngine");

			
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Enable");
				if (pAlarmt.enable)
					strcpy(p_simpleitem->SimpleItem.Value, "true");
				else
					strcpy(p_simpleitem->SimpleItem.Value, "false");
			}
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Type");
				
				if (((access("/opt/ch/flag.pd", F_OK) == 0) || (access("/opt/ch/flag.pd.sstar", F_OK) == 0)) && (access("/opt/ch/flag.car.sstar", F_OK) == 0))
					strcpy(p_simpleitem->SimpleItem.Value, "2");
				else if ((access("/opt/ch/flag.pd", F_OK) == 0) || (access("/opt/ch/flag.pd.sstar", F_OK) == 0))
					strcpy(p_simpleitem->SimpleItem.Value, "1");
				else
					strcpy(p_simpleitem->SimpleItem.Value, "0");
			}
		}
	}
#if 0
	if (0)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.AnalyticsEngineConfiguration.AnalyticsModule);
		if (p_config)
		{
			strcpy(p_config->Config.Name, "MyMotionRegionDetector");
			strcpy(p_config->Config.Type, "tt:MotionRegionDetector");
	
			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
				strcpy(p_simpleitem->SimpleItem.Value, "6");
			}
		}
	}
#endif
	// todo : here init rule engine configuration ...
	
	if (1)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule);
		if (p_config)
		{
			memset(p_config->Config.Name, 0, 100);
			memset(p_config->Config.Type, 0, 100);
			strcpy(p_config->Config.Name, "MyMotionDetectorRule");
			strcpy(p_config->Config.Type, "tt:CellMotionDetector");

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "MinCount");
				strcpy(p_simpleitem->SimpleItem.Value, "5");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOnDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "AlarmOffDelay");
				strcpy(p_simpleitem->SimpleItem.Value, "100");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "ActiveCells");
				strcpy(p_simpleitem->SimpleItem.Value, "zwA=");
				if (1)
				{
					char *value = NULL;
					Aj_Get_ActiveCells(&md_alarm, &value);
					
					if (value != NULL)
					{
						memset(p_simpleitem->SimpleItem.Value, 0, ONVIF_TOKEN_LEN);
						strcpy(p_simpleitem->SimpleItem.Value, value);
						free(value);
					}
				}
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "hb_ext");
				
				p_elementitem->ElementItem.Any = (char *)malloc(10240);
				if (p_elementitem->ElementItem.Any != NULL)
				{
					p_elementitem->ElementItem.AnyFlag = 1;
					int MDA_enable = md_alarm.enable;
					if(MDA_enable == 0)
					{
						sprintf(p_elementitem->ElementItem.Any, "<tt:SimpleItem Name=\"MotionDetectorEnable\" Value=\"false\" />");
					}
					else
					{
						sprintf(p_elementitem->ElementItem.Any, "<tt:SimpleItem Name=\"MotionDetectorEnable\" Value=\"true\" />");
						TimeSpanList timeSpanList;
						TransTimeSpan2Old(&md_alarm.timeSpan, &timeSpanList);
						
						int MDA_WDCnt = timeSpanList.workdayCnt;
						int i = 0;
						for(i = 0; i < MDA_WDCnt; i ++)
						{
							switch(timeSpanList.workdayTimes[i].workday)
							{
								case 0:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Sunday\">");
									break;
								case 1:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Monday\">");
									break;
								case 2:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Tuesday\">");
									break;
								case 3:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Wednesday\">");
									break;
								case 4:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Thursday\">");
									break;
								case 5:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Friday\">");
									break;
								case 6:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Saturday\">");
									break;
								case 7:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
									break;
								default:
									strcat(p_elementitem->ElementItem.Any, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
									break;
							}
							
							int MDA_TSCnt = timeSpanList.workdayTimes[i].timeSpancnt;
							int tIndex = 0;
							for(tIndex = 0; tIndex < MDA_TSCnt; tIndex ++)
							{
								int startH = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.hour;
								int startM = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.minute;
								int startS = timeSpanList.workdayTimes[i].timeSpans[tIndex].startTime.sec;
								int endH = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.hour;
								int endM = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.minute;
								int endS = timeSpanList.workdayTimes[i].timeSpans[tIndex].endTime.sec;
								char timeBUF[100] = {0};
								sprintf(timeBUF, "<tt:SimpleItem Name=\"time_seg\" Value=\"%02d:%02d:%02d-%02d:%02d:%02d\" />", startH, startM, startS, endH, endM, endS);
								strcat(p_elementitem->ElementItem.Any, timeBUF);
							}
							strcat(p_elementitem->ElementItem.Any, "</tt:ElementItem>");
						}
					}
				}
			}
		}
	}
	
	if (g_hasSPD || g_hasSCAR || g_hasSMART)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule);
		if (p_config)
		{
			memset(p_config->Config.Name, 0, 100);
			memset(p_config->Config.Type, 0, 100);
			strcpy(p_config->Config.Name, "MySmartMotionDetector");
			strcpy(p_config->Config.Type, "tt:SmartMotionDetector");

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Enable");
				if (pAlarmt.enable == 0)
					strcpy(p_simpleitem->SimpleItem.Value, "false");
				else
					strcpy(p_simpleitem->SimpleItem.Value, "true");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Sensitivity");
				char Sensitivity_msg[64] = {0};
				sprintf(Sensitivity_msg, "%d", pAlarmt.sensitivity*10 + pAlarmt.threshold);
				strcpy(p_simpleitem->SimpleItem.Value, Sensitivity_msg);
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Reserved");
				strcpy(p_simpleitem->SimpleItem.Value, "0");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Type");
				char type_buf[33] = {0};
				type_buf[32] = '\0';
				ToBin(pAlarmt.type, type_buf);
				if ((type_buf[31-0] == 0x31) && (type_buf[31-4] == 0x31))
					strcpy(p_simpleitem->SimpleItem.Value, "3");
				else if ((type_buf[31-0] == 0x31) && (type_buf[31-4] == 0x30))
					strcpy(p_simpleitem->SimpleItem.Value, "2");
				else if ((type_buf[31-0] == 0x30) && (type_buf[31-4] == 0x31))
					strcpy(p_simpleitem->SimpleItem.Value, "1");
				else if ((type_buf[31-0] == 0x30) && (type_buf[31-4] == 0x30))
					strcpy(p_simpleitem->SimpleItem.Value, "0");
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Field");

				char Field_info[512] = {0};
				char Field_msg1[512] = "<tt:PolygonConfiguration>\n\
                            <tt:Polygon>\n";
				char Field_msg2[128] =  "<tt:Point x=\"%d\" y=\"%d\"/>\n";
				char Field_msg3[128] =  "</tt:Polygon>\n\
                       </tt:PolygonConfiguration>";
				char Field_msg4[128] = {0};
				
				int i = 0;
				for (i = 0; i < pAlarmt.polygonArea.count; i ++)
				{
					sprintf(Field_msg4, Field_msg2, (pAlarmt.polygonArea.points[i].x)*100, (pAlarmt.polygonArea.points[i].y)*100);
					strcat(Field_msg1, Field_msg4);
					memset(Field_msg4, 0, sizeof(Field_msg4));
				}
				
				strcat(Field_msg1, Field_msg3);
				strcat(Field_info, Field_msg1);
				
				p_elementitem->ElementItem.Any = (char *)malloc(strlen(Field_info));
				if (p_elementitem->ElementItem.Any != NULL)
				{
					p_elementitem->ElementItem.AnyFlag = 1;
					strcpy(p_elementitem->ElementItem.Any, Field_info);
				}
				
			}

			p_elementitem = onvif_add_ElementItem(&p_config->Config.Parameters.ElementItem);
			if (p_elementitem)
			{
				strcpy(p_elementitem->ElementItem.Name, "Date");
				
				TimeSpanList timeSpanList1;
				memset(&timeSpanList1, 0, sizeof(TimeSpanList));
				TransTimeSpan2Old(&(pAlarmt.timeSpan), &timeSpanList1);
				char my_pAnyBuf[1024 * 6] = {0};
				int MDA_WDCnt1 = timeSpanList1.workdayCnt;
				
				int i = 0;
				for(i = 0; i < MDA_WDCnt1; i ++)
				{
					switch(timeSpanList1.workdayTimes[i].workday)
					{
						case 0:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Sunday\">");
							break;
						case 1:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Monday\">");
							break;
						case 2:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Tuesday\">");
							break;
						case 3:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Wednesday\">");
							break;
						case 4:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Thursday\">");
							break;
						case 5:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Friday\">");
							break;
						case 6:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Saturday\">");
							break;
						case 7:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
							break;
						default:
							strcat(my_pAnyBuf, "<tt:ElementItem Name=\"Week\" Value=\"Everyday\">");
							break;
					}
					
					int MDA_TSCnt = timeSpanList1.workdayTimes[i].timeSpancnt;
					int tIndex = 0;
					for(tIndex = 0; tIndex < MDA_TSCnt; tIndex ++)
					{
						int startH = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.hour;
						int startM = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.minute;
						int startS = timeSpanList1.workdayTimes[i].timeSpans[tIndex].startTime.sec;
						int endH = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.hour;
						int endM = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.minute;
						int endS = timeSpanList1.workdayTimes[i].timeSpans[tIndex].endTime.sec;
						char timeBUF[100] = {0};
						sprintf(timeBUF, "<tt:SimpleItem Name=\"time_seg\" Value=\"%02d:%02d:%02d-%02d:%02d:%02d\" />",
							startH, startM, startS, endH, endM, endS);
						strcat(my_pAnyBuf, timeBUF);
					}
					strcat(my_pAnyBuf, "</tt:ElementItem>");
				}
				
				p_elementitem->ElementItem.Any = (char *)malloc(strlen(my_pAnyBuf) + 1);
				if (p_elementitem->ElementItem.Any != NULL)
				{
					p_elementitem->ElementItem.AnyFlag = 1;
					strcpy(p_elementitem->ElementItem.Any, my_pAnyBuf);
				}
			}
		}
	}
#if 0
	if (0)
	{
		p_config = onvif_add_Config(&p_va_cfg->Configuration.RuleEngineConfiguration.Rule);
		if (p_config)
		{
			strcpy(p_config->Config.Name, "TamperingDetection");
			strcpy(p_config->Config.Type, "tt:TamperingDetection");

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Mode");
				strcpy(p_simpleitem->SimpleItem.Value, "SignalLoss");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Threshold");
				strcpy(p_simpleitem->SimpleItem.Value, "50.0");
			}

			p_simpleitem = onvif_add_SimpleItem(&p_config->Config.Parameters.SimpleItem);
			if (p_simpleitem)
			{
				strcpy(p_simpleitem->SimpleItem.Name, "Duration");
				strcpy(p_simpleitem->SimpleItem.Value, "PT10S");
			}
		}
	}
#endif 

	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_VideoAnalyticsConfiguration OVER\n\n");
	return;
}

#endif	// end of VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT

HT_API void onvif_get_Recording_token(RecordingList * p_head, char * token, int size)
{
	RecordingList * p_tmp = NULL;
	do {
		snprintf(token, size, "RecordingToken_%u", ++g_onvif_idx.recording_idx);
		p_tmp = onvif_find_Recording(p_head, token);
	} while (p_tmp);
}

HT_API RecordingList * onvif_add_Recording(RecordingList ** p_head)
{
	RecordingList * p_tmp;
	RecordingList * p_new = (RecordingList *) malloc(sizeof(RecordingList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(RecordingList));
	onvif_get_Recording_token(*p_head, p_new->Recording.RecordingToken, sizeof(p_new->Recording.RecordingToken));	
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API RecordingList * onvif_find_Recording(RecordingList * p_head, const char * token)
{
	RecordingList * p_tmp = p_head;
	if (NULL == token || token[0] == '\0')
	{
		return p_tmp;
	}
	while (p_tmp)
	{
		if (strcmp(p_tmp->Recording.RecordingToken, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_Recording(RecordingList ** p_head, RecordingList * p_node)
{
	RecordingList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	onvif_free_Tracks(&p_node->Recording.Tracks);
	free(p_node);
}

HT_API void onvif_free_Recordings(RecordingList ** p_head)
{
	RecordingList * p_next;
	RecordingList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_Tracks(&p_tmp->Recording.Tracks);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_Track_token(TrackList * p_head, char * token, int size)
{
	TrackList * p_tmp = NULL;
	do {
		snprintf(token, size, "TrackToken_%u", ++g_onvif_idx.track_idx);
		p_tmp = onvif_find_Track(p_head, token);
	} while (p_tmp);
}

HT_API TrackList * onvif_add_Track(TrackList ** p_head)
{
	TrackList * p_tmp;
	TrackList * p_new = (TrackList *) malloc(sizeof(TrackList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(TrackList));
	onvif_get_Track_token(*p_head, p_new->Track.TrackToken, sizeof(p_new->Track.TrackToken));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_Track(TrackList ** p_head, TrackList * p_track)
{
	TrackList * p_prev;
	p_prev = *p_head;
	if (p_track == p_prev)
	{
		*p_head = p_track->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_track)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_track->next;
	}
	free(p_track);
}

HT_API void onvif_free_Tracks(TrackList ** p_head)
{
	TrackList * p_next;
	TrackList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API TrackList * onvif_find_Track(TrackList * p_head, const char * token)
{
	TrackList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->Track.TrackToken, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API int	onvif_get_track_nums_by_type(TrackList * p_head, onvif_TrackType type)
{
	int nums = 0;
	TrackList * p_tmp = p_head;
	while (p_tmp)
	{
		if (p_tmp->Track.Configuration.TrackType == type)
		{
			nums++;
		}
		p_tmp = p_tmp->next;
	}
	return nums;
}

HT_API void onvif_get_RecordingJob_token(RecordingJobList * p_head, char * token, int size)
{
	RecordingJobList * p_tmp = NULL;
	do {
		snprintf(token, size, "RecordingJobToken_%u", ++g_onvif_idx.recordingjob_idx);
		p_tmp = onvif_find_RecordingJob(p_head, token);
	} while (p_tmp);
}

HT_API RecordingJobList * onvif_add_RecordingJob(RecordingJobList ** p_head)
{
	RecordingJobList * p_tmp;
	RecordingJobList * p_new = (RecordingJobList *) malloc(sizeof(RecordingJobList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(RecordingJobList));
	onvif_get_RecordingJob_token(*p_head, p_new->RecordingJob.JobToken, sizeof(p_new->RecordingJob.JobToken));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API RecordingJobList * onvif_find_RecordingJob(RecordingJobList * p_head, const char * token)
{
	RecordingJobList * p_tmp = p_head;
	while (p_tmp)
	{
		if (strcmp(p_tmp->RecordingJob.JobToken, token) == 0)
		{
			break;
		}
		p_tmp = p_tmp->next;
	}
	return p_tmp;
}

HT_API void onvif_free_RecordingJob(RecordingJobList ** p_head, RecordingJobList * p_node)
{
	RecordingJobList * p_prev;
	if (NULL == p_node)
	{
		return;
	}
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_RecordingJobs(RecordingJobList ** p_head)
{
	RecordingJobList * p_next;
	RecordingJobList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API RecordingInformationList * onvif_add_RecordingInformation(RecordingInformationList ** p_head)
{
	RecordingInformationList * p_tmp;
	RecordingInformationList * p_new = (RecordingInformationList *) malloc(sizeof(RecordingInformationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(RecordingInformationList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_RecordingInformations(RecordingInformationList ** p_head)
{
	RecordingInformationList * p_next;
	RecordingInformationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API FindEventResultList * onvif_add_FindEventResult(FindEventResultList ** p_head)
{
	FindEventResultList * p_tmp;
	FindEventResultList * p_new = (FindEventResultList *) malloc(sizeof(FindEventResultList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(FindEventResultList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_FindEventResult(FindEventResultList ** p_head, FindEventResultList * p_node)
{
	FindEventResultList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}

	onvif_free_SimpleItems(&p_node->Result.Event.Message.Source.SimpleItem);
	onvif_free_SimpleItems(&p_node->Result.Event.Message.Key.SimpleItem);
	onvif_free_SimpleItems(&p_node->Result.Event.Message.Data.SimpleItem);

	onvif_free_ElementItems(&p_node->Result.Event.Message.Source.ElementItem);
	onvif_free_ElementItems(&p_node->Result.Event.Message.Key.ElementItem);
	onvif_free_ElementItems(&p_node->Result.Event.Message.Data.ElementItem);

	free(p_node);
}

HT_API void onvif_free_FindEventResults(FindEventResultList ** p_head)
{
	FindEventResultList * p_next;
	FindEventResultList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Source.SimpleItem);
		onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Key.SimpleItem);
		onvif_free_SimpleItems(&p_tmp->Result.Event.Message.Data.SimpleItem);

		onvif_free_ElementItems(&p_tmp->Result.Event.Message.Source.ElementItem);
		onvif_free_ElementItems(&p_tmp->Result.Event.Message.Key.ElementItem);
		onvif_free_ElementItems(&p_tmp->Result.Event.Message.Data.ElementItem);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API FindMetadataResultList * onvif_add_FindMetadataResult(FindMetadataResultList ** p_head)
{
	FindMetadataResultList * p_tmp;
	FindMetadataResultList * p_new = (FindMetadataResultList *) malloc(sizeof(FindMetadataResultList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(FindMetadataResultList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_FindMetadataResult(FindMetadataResultList ** p_head, FindMetadataResultList * p_node)
{
	FindMetadataResultList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_FindMetadataResults(FindMetadataResultList ** p_head)
{
	FindMetadataResultList * p_next;
	FindMetadataResultList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API FindPTZPositionResultList * onvif_add_FindPTZPositionResult(FindPTZPositionResultList ** p_head)
{
	FindPTZPositionResultList * p_tmp;
	FindPTZPositionResultList * p_new = (FindPTZPositionResultList *) malloc(sizeof(FindPTZPositionResultList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(FindPTZPositionResultList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_FindPTZPositionResult(FindPTZPositionResultList ** p_head, FindPTZPositionResultList * p_node)
{
	FindPTZPositionResultList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_FindPTZPositionResults(FindPTZPositionResultList ** p_head)
{
	FindPTZPositionResultList * p_next;
	FindPTZPositionResultList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

void onvif_init_Recording()
{
	//log_print(HT_LOG_INFO, "onvif_init_Recording START\n");
	RecordingList * p_recording;
	if (g_onvif_cfg.recordings)
	{
		return;
	}
	p_recording = onvif_add_Recording(&g_onvif_cfg.recordings);
	if (p_recording)
	{
		TrackList * p_track;
		strcpy(p_recording->Recording.Configuration.Source.SourceId, "http://localhost/sourceID");
		strcpy(p_recording->Recording.Configuration.Source.Name, "CameraName");
		strcpy(p_recording->Recording.Configuration.Source.Location, "LocationDescription");
		strcpy(p_recording->Recording.Configuration.Source.Description, "SourceDescription");
		strcpy(p_recording->Recording.Configuration.Source.Address, "http://www.onvif.org/ver10/schema/Profile");
		strcpy(p_recording->Recording.Configuration.Content, "Recording from device");
		p_recording->Recording.Configuration.MaximumRetentionTimeFlag = 1;
		p_recording->Recording.Configuration.MaximumRetentionTime = 0;
		p_recording->EarliestRecording = time(NULL) - 3600;
		p_recording->LatestRecording = time(NULL);

		p_track = onvif_add_Track(&p_recording->Recording.Tracks);
		if (p_track)
		{
			strcpy(p_track->Track.TrackToken, "VIDEO001");
			p_track->Track.Configuration.TrackType = TrackType_Video;

			p_track->EarliestRecording = p_recording->EarliestRecording;
			p_track->LatestRecording = p_recording->LatestRecording;
		}
		p_track = onvif_add_Track(&p_recording->Recording.Tracks);
		if (p_track)
		{
			strcpy(p_track->Track.TrackToken, "AUDIO001");
			p_track->Track.Configuration.TrackType = TrackType_Audio;
			p_track->EarliestRecording = p_recording->EarliestRecording;
			p_track->LatestRecording = p_recording->LatestRecording;
		}
		p_track = onvif_add_Track(&p_recording->Recording.Tracks);
		if (p_track)
		{
			strcpy(p_track->Track.TrackToken, "META001");
			p_track->Track.Configuration.TrackType = TrackType_Metadata;
			p_track->EarliestRecording = p_recording->EarliestRecording;
			p_track->LatestRecording = p_recording->LatestRecording;
		}
	}
	//log_print(HT_LOG_INFO, "onvif_init_Recording OVER\n");
	return;}

void onvif_init_RecordingJob()
{
	//log_print(HT_LOG_INFO, "onvif_init_RecordingJob START\n");

	RecordingJobList * p_recordingjob;
	if (g_onvif_cfg.recording_jobs)
	{
		return;
	}
	p_recordingjob = onvif_add_RecordingJob(&g_onvif_cfg.recording_jobs);
	if (p_recordingjob)
	{
		strcpy(p_recordingjob->RecordingJob.JobConfiguration.Mode, "Active");
		p_recordingjob->RecordingJob.JobConfiguration.Priority = 1;
		if (g_onvif_cfg.recordings)
		{
			strcpy(p_recordingjob->RecordingJob.JobConfiguration.RecordingToken, g_onvif_cfg.recordings->Recording.RecordingToken);
		}
		if (g_onvif_cfg.profiles)
		{
			p_recordingjob->RecordingJob.JobConfiguration.sizeSource = 1;
			p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceTokenFlag = 1;
			p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.TypeFlag = 1;
			strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.Type, "http://www.onvif.org/ver10/schema/Profile");
			strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].SourceToken.Token, g_onvif_cfg.profiles->token);

			p_recordingjob->RecordingJob.JobConfiguration.Source[0].sizeTracks = 1;
			strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].Tracks[0].SourceTag, "SourceTag");
			strcpy(p_recordingjob->RecordingJob.JobConfiguration.Source[0].Tracks[0].Destination, "VIDEO001");
		}
	}
	//log_print(HT_LOG_INFO, "onvif_init_RecordingJob OVER\n");
	return;
}

#endif	// end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT

HT_API void onvif_get_AccessPoint_token(AccessPointList * p_head, char * token, int size)
{
	AccessPointList * p_tmp = NULL;
	do {
		snprintf(token, size, "AccessPointToken_%u", ++g_onvif_idx.aceess_point_idx);
		p_tmp = onvif_find_AccessPoint(p_head, token);
	} while (p_tmp);
}

HT_API AccessPointList * onvif_add_AccessPoint(AccessPointList ** p_head)
{
	AccessPointList * p_tmp;
	AccessPointList * p_new = (AccessPointList *) malloc(sizeof(AccessPointList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AccessPointList));
	onvif_get_AccessPoint_token(*p_head, p_new->AccessPointInfo.token, sizeof(p_new->AccessPointInfo.token));
	sprintf(p_new->AccessPointInfo.Name, "AccessPointName_%u", g_onvif_idx.aceess_point_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AccessPointList * onvif_find_AccessPoint(AccessPointList * p_head, const char * token)
{
	AccessPointList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->AccessPointInfo.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_AccessPoint(AccessPointList ** p_head, AccessPointList * p_node)
{
	AccessPointList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_AccessPoints(AccessPointList ** p_head)
{
	AccessPointList * p_next;
	AccessPointList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API DoorInfoList * onvif_add_DoorInfo(DoorInfoList ** p_head)
{
	DoorInfoList * p_tmp;
	DoorInfoList * p_new = (DoorInfoList *) malloc(sizeof(DoorInfoList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(DoorInfoList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API DoorInfoList * onvif_find_DoorInfo(DoorInfoList * p_head, const char * token)
{
	DoorInfoList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcasecmp(token, p_tmp->DoorInfo.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_DoorInfos(DoorInfoList ** p_head)
{
	DoorInfoList * p_next;
	DoorInfoList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_Door_token(DoorList * p_head, char * token, int size)
{
	DoorList * p_tmp = NULL;
	do {
		snprintf(token, size, "DoorToken_%u", ++g_onvif_idx.door_idx);
		p_tmp = onvif_find_Door(p_head, token);
	} while (p_tmp);
}

HT_API DoorList * onvif_add_Door(DoorList ** p_head)
{
	DoorList * p_tmp;
	DoorList * p_new = (DoorList *) malloc(sizeof(DoorList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(DoorList));
	onvif_get_Door_token(*p_head, p_new->Door.DoorInfo.token, sizeof(p_new->Door.DoorInfo.token));
	sprintf(p_new->Door.DoorInfo.Name, "DoorName_%u", g_onvif_idx.door_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API DoorList * onvif_find_Door(DoorList * p_head, const char * token)
{
	DoorList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Door.DoorInfo.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Door(DoorList ** p_head, DoorList * p_node)
{
	DoorList * p_prev;
	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}
			p_prev = p_prev->next;
		}
		p_prev->next = p_node->next;
	}
	free(p_node);
}

HT_API void onvif_free_Doors(DoorList ** p_head)
{
	DoorList * p_next;
	DoorList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_Area_token(AreaList * p_head, char * token, int size)
{
	AreaList * p_tmp = NULL;
	do {
		snprintf(token, size, "AreaToken_%u", ++g_onvif_idx.area_idx);
		p_tmp = onvif_find_Area(p_head, token);
	} while (p_tmp);
}

HT_API AreaList * onvif_add_Area(AreaList ** p_head)
{
	AreaList * p_tmp;
	AreaList * p_new = (AreaList *) malloc(sizeof(AreaList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AreaList));
	onvif_get_Area_token(*p_head, p_new->AreaInfo.token, sizeof(p_new->AreaInfo.token));
	sprintf(p_new->AreaInfo.Name, "AreaName_%u", g_onvif_idx.area_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AreaList * onvif_find_Area(AreaList * p_head, const char * token)
{
	AreaList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->AreaInfo.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Area(AreaList ** p_head, AreaList * p_node)
{
	AreaList * p_prev;

	p_prev = *p_head;
	if (p_node == p_prev)
	{
		*p_head = p_node->next;
	}
	else
	{
		while (p_prev->next)
		{
			if (p_prev->next == p_node)
			{
				break;
			}

			p_prev = p_prev->next;
		}

		p_prev->next = p_node->next;
	}

	free(p_node);
}

HT_API void onvif_free_Areas(AreaList ** p_head)
{
	AreaList * p_next;
	AreaList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

void onvif_init_AccessPoint(AccessPointList * p_accesspoint, DoorList * p_door, AreaList * p_area)
{
	p_accesspoint->Enabled = TRUE;
	p_accesspoint->AccessPointInfo.DescriptionFlag = 1;
	sprintf(p_accesspoint->AccessPointInfo.Description, "Access point %d", g_onvif_idx.aceess_point_idx);

	if (p_area)
	{
		p_accesspoint->AccessPointInfo.AreaFromFlag = 1;
		strcpy(p_accesspoint->AccessPointInfo.AreaFrom, p_area->AreaInfo.token);

		p_area = p_area->next;
	}

	if (p_area)
	{
		p_accesspoint->AccessPointInfo.AreaToFlag = 1;
		strcpy(p_accesspoint->AccessPointInfo.AreaTo, p_area->AreaInfo.token);
	}

	if (p_door)
	{
		strcpy(p_accesspoint->AccessPointInfo.Entity, p_door->Door.DoorInfo.token);

		p_accesspoint->AccessPointInfo.EntityTypeFlag = 1;
		strcpy(p_accesspoint->AccessPointInfo.EntityType, "tdc:Door");
	}

	p_accesspoint->AccessPointInfo.Capabilities.DisableAccessPoint = TRUE;
	p_accesspoint->AccessPointInfo.Capabilities.Duress = TRUE;
	p_accesspoint->AccessPointInfo.Capabilities.AnonymousAccess = TRUE;
	p_accesspoint->AccessPointInfo.Capabilities.AccessTaken = TRUE;
	p_accesspoint->AccessPointInfo.Capabilities.ExternalAuthorization = FALSE;
}

void onvif_init_AccessPointList()
{
	//log_print(HT_LOG_INFO, "onvif_init_AccessPointList START\n");
	// here, init two access point for two door ...

	DoorList * p_door = g_onvif_cfg.doors;
	AreaList * p_area = g_onvif_cfg.areas;
	AccessPointList * p_accesspoint;

	if (g_onvif_cfg.access_points)
	{
		return;
	}

	p_accesspoint = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
	if (p_accesspoint)
	{
		onvif_init_AccessPoint(p_accesspoint, p_door, p_area);

		if (p_door)
		{
			p_door = p_door->next;
		}

		if (p_area)
		{
			p_area = p_area->next;
		}

		if (p_area)
		{
			p_area = p_area->next;
		}
	}
	p_accesspoint = onvif_add_AccessPoint(&g_onvif_cfg.access_points);
	if (p_accesspoint)
	{
		onvif_init_AccessPoint(p_accesspoint, p_door, p_area);
	}
	//log_print(HT_LOG_INFO, "onvif_init_AccessPointList OVER\n");
	return;
}

void onvif_init_Door(DoorList * p_door)
{
	strcpy(p_door->Door.DoorType, "pt:Door");

	p_door->Door.DoorInfo.DescriptionFlag = 1;
	sprintf(p_door->Door.DoorInfo.Description, "Door %d", g_onvif_idx.door_idx);

	p_door->Door.DoorInfo.Capabilities.Access = TRUE;
	p_door->Door.DoorInfo.Capabilities.AccessTimingOverride = TRUE;
	p_door->Door.DoorInfo.Capabilities.Lock = TRUE;
	p_door->Door.DoorInfo.Capabilities.Unlock = TRUE;
	p_door->Door.DoorInfo.Capabilities.Block = TRUE;
	p_door->Door.DoorInfo.Capabilities.DoubleLock = TRUE;
	p_door->Door.DoorInfo.Capabilities.LockDown = TRUE;
	p_door->Door.DoorInfo.Capabilities.LockOpen = TRUE;
	p_door->Door.DoorInfo.Capabilities.DoorMonitor = TRUE;
	p_door->Door.DoorInfo.Capabilities.LockMonitor = TRUE;
	p_door->Door.DoorInfo.Capabilities.DoubleLockMonitor = TRUE;
	p_door->Door.DoorInfo.Capabilities.Alarm = TRUE;
	p_door->Door.DoorInfo.Capabilities.Tamper = TRUE;
	p_door->Door.DoorInfo.Capabilities.Fault = TRUE;

	p_door->DoorState.DoorPhysicalStateFlag = 1;
	p_door->DoorState.DoorPhysicalState = DoorPhysicalState_Closed;
	p_door->DoorState.LockPhysicalStateFlag = 1;
	p_door->DoorState.LockPhysicalState = LockPhysicalState_Locked;
	p_door->DoorState.DoubleLockPhysicalStateFlag = 1;
	p_door->DoorState.DoubleLockPhysicalState = LockPhysicalState_Locked;
	p_door->DoorState.AlarmFlag = 1;
	p_door->DoorState.Alarm = DoorAlarmState_Normal;
	p_door->DoorState.FaultFlag = 1;
	p_door->DoorState.Fault.State = DoorFaultState_NotInFault;
	p_door->DoorState.DoorMode = DoorMode_Locked;
	p_door->DoorState.TamperFlag = 1;
	p_door->DoorState.Tamper.State = DoorTamperState_NotInTamper;
}

void onvif_init_DoorList()
{
	//log_print(HT_LOG_INFO, "onvif_init_DoorList START\n");
	DoorList * p_door;
	
	if (g_onvif_cfg.doors)
	{
		return;
	}

	// here, init two door ...

	p_door = onvif_add_Door(&g_onvif_cfg.doors);
	if (p_door)
	{
		onvif_init_Door(p_door);
	}

	p_door = onvif_add_Door(&g_onvif_cfg.doors);
	if (p_door)
	{
		onvif_init_Door(p_door);
	}
	//log_print(HT_LOG_INFO, "onvif_init_DoorList OVER\n");
	return;
}

void onvif_init_AreaList()
{
	//log_print(HT_LOG_INFO, "onvif_init_AreaList START\n");
	AreaList * p_info;
	
	if (g_onvif_cfg.areas)
	{
		return;
	}

	// here, init four area for two door ...

	p_info = onvif_add_Area(&g_onvif_cfg.areas);
	if (p_info)
	{
		p_info->AreaInfo.DescriptionFlag = 1;
		sprintf(p_info->AreaInfo.Description, "Area %d", g_onvif_idx.area_idx);
	}

	p_info = onvif_add_Area(&g_onvif_cfg.areas);
	if (p_info)
	{
		p_info->AreaInfo.DescriptionFlag = 1;
		sprintf(p_info->AreaInfo.Description, "Area %d", g_onvif_idx.area_idx);
	}

	p_info = onvif_add_Area(&g_onvif_cfg.areas);
	if (p_info)
	{
		p_info->AreaInfo.DescriptionFlag = 1;
		sprintf(p_info->AreaInfo.Description, "Area %d", g_onvif_idx.area_idx);
	}

	p_info = onvif_add_Area(&g_onvif_cfg.areas);
	if (p_info)
	{
		p_info->AreaInfo.DescriptionFlag = 1;
		sprintf(p_info->AreaInfo.Description, "Area %d", g_onvif_idx.area_idx);
	}
	//log_print(HT_LOG_INFO, "onvif_init_AreaList OVER\n");
	return;
}

#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT

HT_API PaneLayoutList * onvif_add_PaneLayout(PaneLayoutList ** p_head)
{
	PaneLayoutList * p_tmp;
	PaneLayoutList * p_new = (PaneLayoutList *) malloc(sizeof(PaneLayoutList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(PaneLayoutList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}	
	return p_new;
}

HT_API PaneLayoutList * onvif_find_PaneLayout(PaneLayoutList * p_head, const char * token)
{
	PaneLayoutList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->PaneLayout.Pane) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_PaneLayouts(PaneLayoutList ** p_head)
{
	PaneLayoutList * p_next;
	PaneLayoutList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoOutput_token(VideoOutputList * p_head, char * token, int size)
{
	VideoOutputList * p_tmp = NULL;
	do {
		snprintf(token, size, "VideoOutputToken_%u", ++g_onvif_idx.v_out_idx);
		p_tmp = onvif_find_VideoOutput(p_head, token);
	} while (p_tmp);
}

HT_API VideoOutputList * onvif_add_VideoOutput(VideoOutputList ** p_head)
{
	VideoOutputList * p_tmp;
	VideoOutputList * p_new = (VideoOutputList *) malloc(sizeof(VideoOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoOutputList));
	onvif_get_VideoOutput_token(*p_head, p_new->VideoOutput.token, sizeof(p_new->VideoOutput.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoOutputList * onvif_find_VideoOutput(VideoOutputList * p_head, const char * token)
{
	VideoOutputList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->VideoOutput.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_VideoOutputs(VideoOutputList ** p_head)
{
	VideoOutputList * p_next;
	VideoOutputList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		onvif_free_PaneLayouts(&p_tmp->VideoOutput.Layout.PaneLayout);
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_VideoOutputConfiguration_token(VideoOutputConfigurationList * p_head, char * token, int size)
{
	VideoOutputConfigurationList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "VideoOutputConfigurationToken_%u", ++g_onvif_idx.v_out_cfg_idx);
		p_tmp = onvif_find_VideoOutputConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API VideoOutputConfigurationList * onvif_add_VideoOutputConfiguration(VideoOutputConfigurationList ** p_head)
{
	VideoOutputConfigurationList * p_tmp;
	VideoOutputConfigurationList * p_new = (VideoOutputConfigurationList *) malloc(sizeof(VideoOutputConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(VideoOutputConfigurationList));
	onvif_get_VideoOutputConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "VideoOutputConfigurationName_%u", g_onvif_idx.v_out_cfg_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API VideoOutputConfigurationList * onvif_find_VideoOutputConfiguration(VideoOutputConfigurationList * p_head, const char * token)
{
	VideoOutputConfigurationList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Configuration.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API VideoOutputConfigurationList * onvif_find_VideoOutputConfiguration_by_OutputToken(VideoOutputConfigurationList * p_head, const char * token)
{
	VideoOutputConfigurationList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Configuration.OutputToken) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_VideoOutputConfigurations(VideoOutputConfigurationList ** p_head)
{
	VideoOutputConfigurationList * p_next;
	VideoOutputConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_AudioOutput_token(AudioOutputList * p_head, char * token, int size)
{
	AudioOutputList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "AudioOutputToken_%u", ++g_onvif_idx.a_out_idx);
		p_tmp = onvif_find_AudioOutput(p_head, token);
	} while (p_tmp);
}

HT_API AudioOutputList * onvif_add_AudioOutput(AudioOutputList ** p_head)
{
	AudioOutputList * p_tmp;
	AudioOutputList * p_new = (AudioOutputList *) malloc(sizeof(AudioOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioOutputList));
	onvif_get_AudioOutput_token(*p_head, p_new->AudioOutput.token, sizeof(p_new->AudioOutput.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioOutputList * onvif_find_AudioOutput(AudioOutputList * p_head, const char * token)
{
	AudioOutputList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->AudioOutput.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_AudioOutputs(AudioOutputList ** p_head)
{
	AudioOutputList * p_next;
	AudioOutputList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_AudioOutputConfiguration_token(AudioOutputConfigurationList * p_head, char * token, int size)
{
	AudioOutputConfigurationList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "AudioOutputConfigurationToken_%u", ++g_onvif_idx.a_out_cfg_idx);
		p_tmp = onvif_find_AudioOutputConfiguration(p_head, token);
	} while (p_tmp);
}

HT_API AudioOutputConfigurationList * onvif_add_AudioOutputConfiguration(AudioOutputConfigurationList ** p_head)
{
	AudioOutputConfigurationList * p_tmp;
	AudioOutputConfigurationList * p_new = (AudioOutputConfigurationList *) malloc(sizeof(AudioOutputConfigurationList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AudioOutputConfigurationList));
	onvif_get_AudioOutputConfiguration_token(*p_head, p_new->Configuration.token, sizeof(p_new->Configuration.token));
	sprintf(p_new->Configuration.Name, "AudioOutputConfigurationName_%u", g_onvif_idx.a_out_cfg_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API AudioOutputConfigurationList * onvif_find_AudioOutputConfiguration(AudioOutputConfigurationList * p_head, const char * token)
{
	AudioOutputConfigurationList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Configuration.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API AudioOutputConfigurationList * onvif_find_AudioOutputConfiguration_by_OutputToken(AudioOutputConfigurationList * p_head, const char * token)
{
	AudioOutputConfigurationList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Configuration.OutputToken) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_AudioOutputConfigurations(AudioOutputConfigurationList ** p_head)
{
	AudioOutputConfigurationList * p_next;
	AudioOutputConfigurationList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_RelayOutput_token(RelayOutputList * p_head, char * token, int size)
{
	RelayOutputList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "RelayOutputToken_%u", ++g_onvif_idx.relay_idx);
		p_tmp = onvif_find_RelayOutput(p_head, token);
	} while (p_tmp);
}

HT_API RelayOutputList * onvif_add_RelayOutput(RelayOutputList ** p_head)
{
	RelayOutputList * p_tmp;
	RelayOutputList * p_new = (RelayOutputList *) malloc(sizeof(RelayOutputList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(RelayOutputList));
	onvif_get_RelayOutput_token(*p_head, p_new->RelayOutput.token, sizeof(p_new->RelayOutput.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API RelayOutputList * onvif_find_RelayOutput(RelayOutputList * p_head, const char * token)
{
	RelayOutputList * p_tmp = p_head;
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->RelayOutput.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_RelayOutputs(RelayOutputList ** p_head)
{
	RelayOutputList * p_next;
	RelayOutputList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_DigitalInput_token(DigitalInputList * p_head, char * token, int size)
{
	DigitalInputList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "DigitalInputToken_%u", ++g_onvif_idx.digit_input_idx);
		p_tmp = onvif_find_DigitalInput(p_head, token);
	} while (p_tmp);
}

HT_API DigitalInputList * onvif_add_DigitalInput(DigitalInputList ** p_head)
{
	DigitalInputList * p_tmp;
	DigitalInputList * p_new = (DigitalInputList *) malloc(sizeof(DigitalInputList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(DigitalInputList));
	onvif_get_DigitalInput_token(*p_head, p_new->DigitalInput.token, sizeof(p_new->DigitalInput.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API DigitalInputList * onvif_find_DigitalInput(DigitalInputList * p_head, const char * token)
{
	DigitalInputList * p_tmp = p_head;
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->DigitalInput.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_DigitalInputs(DigitalInputList ** p_head)
{
	DigitalInputList * p_next;
	DigitalInputList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_SerialPort_token(SerialPortList * p_head, char * token, int size)
{
	SerialPortList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "SerialPortToken_%u", ++g_onvif_idx.serial_port_idx);
		p_tmp = onvif_find_SerialPort(p_head, token);
	} while (p_tmp);
}

HT_API SerialPortList * onvif_add_SerialPort(SerialPortList ** p_head)
{
	SerialPortList * p_tmp;
	SerialPortList * p_new = (SerialPortList *) malloc(sizeof(SerialPortList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(SerialPortList));
	onvif_get_SerialPort_token(*p_head, p_new->SerialPort.token, sizeof(p_new->SerialPort.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API SerialPortList * onvif_find_SerialPort(SerialPortList * p_head, const char * token)
{
	SerialPortList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->SerialPort.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API SerialPortList * onvif_find_SerialPort_by_ConfigurationToken(SerialPortList * p_head, const char * token)
{
	SerialPortList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Configuration.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_SerialPorts(SerialPortList ** p_head)
{
	SerialPortList * p_next;
	SerialPortList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_malloc_SerialData(onvif_SerialData * p_data, int union_SerialData, int size)
{
	if (NULL == p_data)
	{
		return;
	}
	if (union_SerialData == 0)
	{
		p_data->_union_SerialData = 0;
		p_data->union_SerialData.Binary = (char *)malloc(size);
		if (p_data->union_SerialData.Binary)
		{
			memset(p_data->union_SerialData.Binary, 0, size);
		}
	}
	else
	{
		p_data->_union_SerialData = 1;
		p_data->union_SerialData.String = (char *)malloc(size);
		if (p_data->union_SerialData.String)
		{
			memset(p_data->union_SerialData.String, 0, size);
		}
	}
}

HT_API void onvif_free_SerialData(onvif_SerialData * p_data)
{
	if (NULL == p_data)
	{
		return;
	}

	if (p_data->_union_SerialData == 0)
	{
		if (p_data->union_SerialData.Binary)
		{
			free(p_data->union_SerialData.Binary);
			p_data->union_SerialData.Binary = NULL;
		}
	}
	else
	{
		if (p_data->union_SerialData.String)
		{
			free(p_data->union_SerialData.String);
			p_data->union_SerialData.String = NULL;
		}
	}
}

#ifdef AUDIO_SUPPORT
void onvif_init_AudioOutput()
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioOutput START\n");
	
	if (g_onvif_cfg.a_output)
	{
		return;
	}
	onvif_add_AudioOutput(&g_onvif_cfg.a_output);
	
	//log_print(HT_LOG_INFO, "onvif_init_AudioOutput OVER\n");
	return;
}

void onvif_init_AudioOutputConfiguration(AudioOutputList * p_output)
{
	//log_print(HT_LOG_INFO, "onvif_init_AudioOutputConfiguration START\n");
	AudioOutputConfigurationList * p_cfg;
	if (g_onvif_cfg.a_output_cfg || !p_output)
	{
		return;
	}
	p_cfg = onvif_add_AudioOutputConfiguration(&g_onvif_cfg.a_output_cfg);

	strcpy(p_cfg->Configuration.OutputToken, p_output->AudioOutput.token);
	p_cfg->Configuration.SendPrimacyFlag = 1;
	strcpy(p_cfg->Configuration.SendPrimacy, "www.onvif.org/ver20/HalfDuplex/Server");
	p_cfg->Configuration.OutputLevel = 100;

	p_cfg->Options.sizeOutputTokensAvailable = 1;
	strcpy(p_cfg->Options.OutputTokensAvailable[0], p_output->AudioOutput.token);
	p_cfg->Options.sizeSendPrimacyOptions = 3;
	strcpy(p_cfg->Options.SendPrimacyOptions[0], "www.onvif.org/ver20/HalfDuplex/Server");
	strcpy(p_cfg->Options.SendPrimacyOptions[1], "www.onvif.org/ver20/HalfDuplex/Client");
	strcpy(p_cfg->Options.SendPrimacyOptions[2], "www.onvif.org/ver20/HalfDuplex/Auto");

	p_cfg->Options.OutputLevelRange.Min = 0;
	p_cfg->Options.OutputLevelRange.Max = 100;
	
	//log_print(HT_LOG_INFO, "onvif_init_AudioOutputConfiguration OVER\n");
	return;}
#endif

void onvif_init_RelayOutput()
{
	//log_print(HT_LOG_INFO, "onvif_init_RelayOutput START\n");
	RelayOutputList * p_output;
	if (!ALARM_SUPPORT_IO_OUT)
	{
		return;
	}
	if (g_onvif_cfg.relay_output)
	{
		return;
	}
	
	OutPutAlarm OutputAlm;
	memcpy(&OutputAlm, &((AlarmConfig *)getAlarmConfig())->normalAlarm.outputAlarm, sizeof(OutputAlm));
	
	OutputChannel *poutputChannels;
	int i;
	OutputAlm.channelCnt = 1;
	for (i = 0; i < OutputAlm.channelCnt && i < MAX_OUTPUT_CHANENL_COUNT && i < MAX_RELAYS ; i++)
	{
		poutputChannels = &(OutputAlm.outputChannels[i]);
		
		p_output = onvif_add_RelayOutput(&g_onvif_cfg.relay_output);

		p_output->RelayOutput.Properties.Mode = RelayMode_Monostable;
		p_output->RelayOutput.Properties.DelayTime = poutputChannels->duration;
		
		if(strstr(poutputChannels->triggerType.name, "HIGH"))
			p_output->RelayOutput.Properties.IdleState = RelayIdleState_open;
		else
			p_output->RelayOutput.Properties.IdleState = RelayIdleState_closed;
		
		p_output->RelayLogicalState = RelayLogicalState_active;
		
		strcpy(p_output->Options.token, p_output->RelayOutput.token);

		p_output->Options.RelayMode_BistableFlag = 1;
		p_output->Options.RelayMode_MonostableFlag = 1;

		p_output->Options.DelayTimesFlag = 1;
		strcpy(p_output->Options.DelayTimes, "0 60000");
	}
	
	//log_print(HT_LOG_INFO, "onvif_init_AreaList OVER\n");
	return;}

void onvif_init_DigitInput()
{
	//log_print(HT_LOG_INFO, "onvif_init_DigitInput START\n");
	DigitalInputList * p_input;
	if (g_onvif_cfg.digit_input)
	{
		return;
	}
	p_input = onvif_add_DigitalInput(&g_onvif_cfg.digit_input);

	p_input->DigitalInput.IdleStateFlag = 1;
	p_input->DigitalInput.IdleState = DigitalIdleState_open;

	p_input->Options.DigitalIdleState_openFlag = 1;
	p_input->Options.DigitalIdleState_closedFlag = 1;
	//log_print(HT_LOG_INFO, "onvif_init_DigitInput OVER\n");
	return;}

void onvif_init_SerialPort()
{
	//log_print(HT_LOG_INFO, "onvif_init_SerialPort START\n");
	SerialPortList * p_port;
	if (g_onvif_cfg.serial_port)
	{
		return;
	}
	p_port = onvif_add_SerialPort(&g_onvif_cfg.serial_port);

	p_port->Configuration.BaudRate = 115200;
	p_port->Configuration.CharacterLength = 8;
	p_port->Configuration.StopBit = 1;
	p_port->Configuration.ParityBit = ParityBit_Odd;
	p_port->Configuration.type = SerialPortType_RS485FullDuplex;

	strcpy(p_port->Options.token, p_port->SerialPort.token);

	p_port->Options.BaudRateList.sizeItems = 2;
	p_port->Options.BaudRateList.Items[0] = 115200;
	p_port->Options.BaudRateList.Items[1] = 98000;

	p_port->Options.CharacterLengthList.sizeItems = 1;
	p_port->Options.CharacterLengthList.Items[0] = 8;

	p_port->Options.ParityBitList.sizeItems = 2;
	p_port->Options.ParityBitList.Items[0] = ParityBit_Odd;
	p_port->Options.ParityBitList.Items[1] = ParityBit_Even;

	p_port->Options.StopBitList.sizeItems = 1;
	p_port->Options.StopBitList.Items[0] = 1;
	
	//log_print(HT_LOG_INFO, "onvif_init_SerialPort OVER\n");
	return;
}

#endif // end of DEVICEIO_SUPPORT

#ifdef THERMAL_SUPPORT

HT_API void onvif_get_ColorPalette_token(ColorPaletteList * p_head, char * token, int size)
{
	ColorPaletteList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "ColorPaletteToken_%u", ++g_onvif_idx.color_palette_idx);
		p_tmp = onvif_find_ColorPalette(p_head, token);
	} while (p_tmp);
}

HT_API ColorPaletteList * onvif_add_ColorPalette(ColorPaletteList ** p_head)
{
	ColorPaletteList * p_tmp;
	ColorPaletteList * p_new = (ColorPaletteList *) malloc(sizeof(ColorPaletteList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ColorPaletteList));
	onvif_get_ColorPalette_token(*p_head, p_new->ColorPalette.token, sizeof(p_new->ColorPalette.token));
	sprintf(p_new->ColorPalette.Name, "ColorPaletteName_%u", g_onvif_idx.color_palette_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API ColorPaletteList * onvif_find_ColorPalette(ColorPaletteList * p_head, const char * token)
{
	ColorPaletteList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->ColorPalette.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_ColorPalettes(ColorPaletteList ** p_head)
{
	ColorPaletteList * p_next;
	ColorPaletteList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API void onvif_get_NUCTable_token(NUCTableList * p_head, char * token, int size)
{
	NUCTableList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "NUCTableToken_%u", ++g_onvif_idx.nuctable_idx);
		p_tmp = onvif_find_NUCTable(p_head, token);
	} while (p_tmp);
}

HT_API NUCTableList * onvif_add_NUCTable(NUCTableList ** p_head)
{
	NUCTableList * p_tmp;
	NUCTableList * p_new = (NUCTableList *) malloc(sizeof(NUCTableList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(NUCTableList));
	onvif_get_NUCTable_token(*p_head, p_new->NUCTable.token, sizeof(p_new->NUCTable.token));
	sprintf(p_new->NUCTable.Name, "NUCTableName_%u", g_onvif_idx.nuctable_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API NUCTableList * onvif_find_NUCTable(NUCTableList * p_head, const char * token)
{
	NUCTableList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->NUCTable.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_NUCTables(NUCTableList ** p_head)
{
	NUCTableList * p_next;
	NUCTableList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

void onvif_init_ThermalConfiguration(onvif_ThermalConfiguration * p_req)
{
	strcpy(p_req->ColorPalette.token, "ColorPaletteToken_1");
	strcpy(p_req->ColorPalette.Name, "ColorPaletteName_1");
	strcpy(p_req->ColorPalette.Type, "WhiteHot");

	p_req->Polarity = Polarity_WhiteHot;

	p_req->NUCTableFlag = 1;
	strcpy(p_req->NUCTable.token, "NUCTableToken_1");
	strcpy(p_req->NUCTable.Name, "NUCTableName_1");
	p_req->NUCTable.LowTemperatureFlag = 1;
	p_req->NUCTable.LowTemperature = 0;
	p_req->NUCTable.HighTemperatureFlag = 1;
	p_req->NUCTable.HighTemperature = 100;

	p_req->CoolerFlag = 1;
	p_req->Cooler.Enabled = TRUE;
	p_req->Cooler.RunTimeFlag = 1;
	p_req->Cooler.RunTime = 0;
}

void onvif_init_ThermalConfigurationOptions(onvif_ThermalConfigurationOptions * p_req)
{
	ColorPaletteList * p_ColorPalette1;
	ColorPaletteList * p_ColorPalette2;
	NUCTableList * p_NUCTable1;
	NUCTableList * p_NUCTable2;

	p_ColorPalette1 = onvif_add_ColorPalette(&p_req->ColorPalette);
	strcpy(p_ColorPalette1->ColorPalette.Type, "WhiteHot");

	p_ColorPalette2 = onvif_add_ColorPalette(&p_req->ColorPalette);
	strcpy(p_ColorPalette2->ColorPalette.Type, "BlackHot");

	p_NUCTable1 = onvif_add_NUCTable(&p_req->NUCTable);
	p_NUCTable1->NUCTable.LowTemperatureFlag = 1;
	p_NUCTable1->NUCTable.LowTemperature = 0;
	p_NUCTable1->NUCTable.HighTemperatureFlag = 1;
	p_NUCTable1->NUCTable.HighTemperature = 100;

	p_NUCTable2 = onvif_add_NUCTable(&p_req->NUCTable);
	p_NUCTable2->NUCTable.LowTemperatureFlag = 1;
	p_NUCTable2->NUCTable.LowTemperature = 0;
	p_NUCTable2->NUCTable.HighTemperatureFlag = 1;
	p_NUCTable2->NUCTable.HighTemperature = 100;

	p_req->CoolerOptionsFlag = 1;
	p_req->CoolerOptions.Enabled = TRUE;
}

void onvif_init_RadiometryConfiguration(onvif_RadiometryConfiguration * p_req)
{
	p_req->RadiometryGlobalParametersFlag = 1;
	p_req->RadiometryGlobalParameters.ReflectedAmbientTemperature = 10;
	p_req->RadiometryGlobalParameters.Emissivity = 10;
	p_req->RadiometryGlobalParameters.DistanceToObject = 10;
	p_req->RadiometryGlobalParameters.RelativeHumidityFlag = 1;
	p_req->RadiometryGlobalParameters.RelativeHumidity = 10;
	p_req->RadiometryGlobalParameters.AtmosphericTemperatureFlag = 1;
	p_req->RadiometryGlobalParameters.AtmosphericTemperature = 10;
	p_req->RadiometryGlobalParameters.AtmosphericTransmittanceFlag = 1;
	p_req->RadiometryGlobalParameters.AtmosphericTransmittance = 10;
	p_req->RadiometryGlobalParameters.ExtOpticsTemperatureFlag = 1;
	p_req->RadiometryGlobalParameters.ExtOpticsTemperature = 10;
	p_req->RadiometryGlobalParameters.ExtOpticsTransmittanceFlag = 1;
	p_req->RadiometryGlobalParameters.ExtOpticsTransmittance = 10;
}

void onvif_init_RadiometryConfigurationOptions(onvif_RadiometryConfigurationOptions * p_req)
{
	p_req->RadiometryGlobalParameterOptionsFlag = 1;
	p_req->RadiometryGlobalParameterOptions.ReflectedAmbientTemperature.Min = 0;
	p_req->RadiometryGlobalParameterOptions.ReflectedAmbientTemperature.Max = 100;
	p_req->RadiometryGlobalParameterOptions.Emissivity.Min = 0;
	p_req->RadiometryGlobalParameterOptions.Emissivity.Max = 100;
	p_req->RadiometryGlobalParameterOptions.DistanceToObject.Min = 0;
	p_req->RadiometryGlobalParameterOptions.DistanceToObject.Max = 100;
	p_req->RadiometryGlobalParameterOptions.RelativeHumidityFlag = 1;
	p_req->RadiometryGlobalParameterOptions.RelativeHumidity.Min = 0;
	p_req->RadiometryGlobalParameterOptions.RelativeHumidity.Max = 100;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTemperatureFlag = 1;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTemperature.Min = 0;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTemperature.Max = 100;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittanceFlag = 1;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittance.Min = 0;
	p_req->RadiometryGlobalParameterOptions.AtmosphericTransmittance.Max = 100;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperatureFlag = 1;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperature.Min = 0;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTemperature.Max = 100;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittanceFlag = 1;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittance.Min = 0;
	p_req->RadiometryGlobalParameterOptions.ExtOpticsTransmittance.Max = 100;
}

HT_API BOOL onvif_init_Thermal(VideoSourceList * p_req)
{
	onvif_init_ThermalConfiguration(&p_req->ThermalConfiguration);
	onvif_init_ThermalConfigurationOptions(&p_req->ThermalConfigurationOptions);
	onvif_init_RadiometryConfiguration(&p_req->RadiometryConfiguration);
	onvif_init_RadiometryConfigurationOptions(&p_req->RadiometryConfigurationOptions);

	return TRUE;
}

#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT

HT_API void onvif_get_Credential_token(CredentialList * p_head, char * token, int size)
{
	CredentialList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "CredentialToken_%u", ++g_onvif_idx.credential_idx);
		p_tmp = onvif_find_Credential(p_head, token);
	} while (p_tmp);
}

HT_API CredentialList * onvif_add_Credential(CredentialList ** p_head)
{
	CredentialList * p_tmp;
	CredentialList * p_new = (CredentialList *) malloc(sizeof(CredentialList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(CredentialList));
	onvif_get_Credential_token(*p_head, p_new->Credential.token, sizeof(p_new->Credential.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next)
			p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API CredentialList * onvif_find_Credential(CredentialList * p_head, const char * token)
{
	CredentialList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Credential.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Credential(CredentialList ** p_head, CredentialList * p_node)
{
	BOOL found = FALSE;
	CredentialList * p_prev = NULL;
	CredentialList * p_tmp = *p_head;	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}
		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
		free(p_tmp);
	}
}

HT_API void onvif_free_Credentials(CredentialList ** p_head)
{
	CredentialList * p_next;
	CredentialList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API BOOL onvif_init_Credential()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_Credential START\n");
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||\n");
	CredentialList * p_tmp;
	if (g_onvif_cfg.credential)
	{
		return TRUE;
	}
	p_tmp = onvif_add_Credential(&g_onvif_cfg.credential);
	if (p_tmp)
	{
		p_tmp->Credential.DescriptionFlag = 1;
		strcpy(p_tmp->Credential.Description, "Credentia");
		strcpy(p_tmp->Credential.CredentialHolderReference, "testuser");

		p_tmp->Credential.sizeCredentialIdentifier = 1;
		p_tmp->Credential.CredentialIdentifier[0].Used = TRUE;
		p_tmp->Credential.CredentialIdentifier[0].ExemptedFromAuthentication = FALSE;
		strcpy(p_tmp->Credential.CredentialIdentifier[0].Type.Name, "pt:Card");
		strcpy(p_tmp->Credential.CredentialIdentifier[0].Type.FormatType, "GUID");
		strcpy(p_tmp->Credential.CredentialIdentifier[0].Value, "31343031303834323633000000000000");
#ifdef ACCESS_RULES
		if (g_onvif_cfg.access_rules)
		{
			p_tmp->Credential.sizeCredentialAccessProfile = 1;
			p_tmp->Credential.CredentialAccessProfile[0].Used = 1;
			strcpy(p_tmp->Credential.CredentialAccessProfile[0].AccessProfileToken, g_onvif_cfg.access_rules->AccessProfile.token);
		}
#endif
		p_tmp->State.AntipassbackStateFlag = 1;
	}
	else
	{
		return FALSE;
	}
	//log_print(HT_LOG_INFO, "\t onvif_init_Credential OVER\n\n");
	return TRUE;
}

HT_API CredentialIdentifierItemList * onvif_add_CredentialIdentifierItem(CredentialIdentifierItemList ** p_head)
{
	CredentialIdentifierItemList * p_tmp;
	CredentialIdentifierItemList * p_new = (CredentialIdentifierItemList *) malloc(sizeof(CredentialIdentifierItemList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(CredentialIdentifierItemList));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API void onvif_free_CredentialIdentifierItem(CredentialIdentifierItemList ** p_head, CredentialIdentifierItemList * p_node)
{
	BOOL found = FALSE;
	CredentialIdentifierItemList * p_prev = NULL;
	CredentialIdentifierItemList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (memcpy(&p_tmp->Item, &p_node->Item, sizeof(onvif_CredentialIdentifierItem)) == 0)
		{
			found = TRUE;
			break;
		}
		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
		free(p_tmp);
	}
}

HT_API void onvif_free_CredentialIdentifierItems(CredentialIdentifierItemList ** p_head)
{
	CredentialIdentifierItemList * p_next;
	CredentialIdentifierItemList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES

HT_API void onvif_get_AccessProfile_token(AccessProfileList * p_head, char * token, int size)
{
	AccessProfileList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "AccessProfileToken_%u", ++g_onvif_idx.accessrule_idx);
		p_tmp = onvif_find_AccessProfile(p_head, token);
	} while (p_tmp);
}

HT_API AccessProfileList * onvif_add_AccessProfile(AccessProfileList ** p_head)
{
	AccessProfileList * p_tmp;
	AccessProfileList * p_new = (AccessProfileList *) malloc(sizeof(AccessProfileList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(AccessProfileList));
	onvif_get_AccessProfile_token(*p_head, p_new->AccessProfile.token, sizeof(p_new->AccessProfile.token));
	sprintf(p_new->AccessProfile.Name, "AccessProfileName_%u", g_onvif_idx.accessrule_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}	
	return p_new;
}

HT_API AccessProfileList * onvif_find_AccessProfile(AccessProfileList * p_head, const char * token)
{
	AccessProfileList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->AccessProfile.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_AccessProfile(AccessProfileList ** p_head, AccessProfileList * p_node)
{
	BOOL found = FALSE;
	AccessProfileList * p_prev = NULL;
	AccessProfileList * p_tmp = *p_head;	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}
		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
		free(p_tmp);
	}
}

HT_API void onvif_free_AccessProfiles(AccessProfileList ** p_head)
{
	AccessProfileList * p_next;
	AccessProfileList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
		
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API BOOL onvif_init_AccessProfile()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_AccessProfile START\n");
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||\n");
	AccessProfileList * p_tmp;
	if (g_onvif_cfg.access_rules)
	{
		return TRUE;
	}
	p_tmp = onvif_add_AccessProfile(&g_onvif_cfg.access_rules);
	if (p_tmp)
	{
		p_tmp->AccessProfile.DescriptionFlag = 1;
		sprintf(p_tmp->AccessProfile.Description, "test");

		p_tmp->AccessProfile.sizeAccessPolicy = 1;
		strcpy(p_tmp->AccessProfile.AccessPolicy[0].ScheduleToken, "test");

#ifdef PROFILE_C_SUPPORT
		if (g_onvif_cfg.access_points)
		{
			strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, g_onvif_cfg.access_points->AccessPointInfo.token);
		}
		else
		{
			strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, "test");
		}
#else
		strcpy(p_tmp->AccessProfile.AccessPolicy[0].Entity, "test");
#endif
	}
	else
	{
		return FALSE;
	}
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_AccessProfile OVER\n\n");
	
	return TRUE;
}

#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT

HT_API void onvif_get_Schedule_token(ScheduleList * p_head, char * token, int size)
{
	ScheduleList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "ScheduleToken_%u", ++g_onvif_idx.schedule_idx);
		p_tmp = onvif_find_Schedule(p_head, token);
	} while (p_tmp);
}

HT_API ScheduleList * onvif_add_Schedule(ScheduleList ** p_head)
{
	ScheduleList * p_tmp;
	ScheduleList * p_new = (ScheduleList *) malloc(sizeof(ScheduleList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ScheduleList));
	onvif_get_Schedule_token(*p_head, p_new->Schedule.token, sizeof(p_new->Schedule.token));
	sprintf(p_new->Schedule.Name, "ScheduleName_%u", g_onvif_idx.schedule_idx);
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API ScheduleList * onvif_find_Schedule(ScheduleList * p_head, const char * token)
{
	ScheduleList * p_tmp = p_head;
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Schedule.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Schedule(ScheduleList ** p_head, ScheduleList * p_node)
{
	BOOL found = FALSE;
	ScheduleList * p_prev = NULL;
	ScheduleList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}
		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
#ifdef LIBICAL
		if (p_tmp->comp)
		{
			icalcomponent_free(p_tmp->comp);
		}
#endif
		free(p_tmp);
	}
}

HT_API void onvif_free_Schedules(ScheduleList ** p_head)
{
	ScheduleList * p_next;
	ScheduleList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
#ifdef LIBICAL
		if (p_tmp->comp)
		{
			icalcomponent_free(p_tmp->comp);
		}
#endif
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API BOOL onvif_init_Schedule()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_Schedule START\n");
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||\n");
	ScheduleList * p_tmp;
	if (g_onvif_cfg.schedule)
	{
		return TRUE;
	}
	p_tmp = onvif_add_Schedule(&g_onvif_cfg.schedule);
	if (p_tmp)
	{
		p_tmp->Schedule.DescriptionFlag = 1;
		sprintf(p_tmp->Schedule.Description, "Schedule %d", g_onvif_idx.schedule_idx);
		sprintf(p_tmp->Schedule.Standard, 
			"BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nDTSTART:20171125T200000\r\n"
			"DTEND:20171126T020000\r\nEND:VEVENT\r\nEND:VCALENDAR");
	}
	else
	{
		return FALSE;
	}
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_Schedule OVER\n\n");
	return TRUE;
}

HT_API void onvif_get_SpecialDayGroup_token(SpecialDayGroupList * p_head, char * token, int size)
{
	SpecialDayGroupList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "SpecialDayGroupToken_%u", ++g_onvif_idx.specialdaygroup_idx);
		p_tmp = onvif_find_SpecialDayGroup(p_head, token);
	} while (p_tmp);
}

HT_API SpecialDayGroupList * onvif_add_SpecialDayGroup(SpecialDayGroupList ** p_head)
{
	SpecialDayGroupList * p_tmp;
	SpecialDayGroupList * p_new = (SpecialDayGroupList *) malloc(sizeof(SpecialDayGroupList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(SpecialDayGroupList));
	onvif_get_SpecialDayGroup_token(*p_head, p_new->SpecialDayGroup.token, sizeof(p_new->SpecialDayGroup.token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;
		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API SpecialDayGroupList * onvif_find_SpecialDayGroup(SpecialDayGroupList * p_head, const char * token)
{
	SpecialDayGroupList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->SpecialDayGroup.token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_SpecialDayGroup(SpecialDayGroupList ** p_head, SpecialDayGroupList * p_node)
{
	BOOL found = FALSE;
	SpecialDayGroupList * p_prev = NULL;
	SpecialDayGroupList * p_tmp = *p_head;	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}
		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}
	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
#ifdef LIBICAL
		if (p_tmp->comp)
		{
			icalcomponent_free(p_tmp->comp);
		}
#endif
		free(p_tmp);
	}
}

HT_API void onvif_free_SpecialDayGroups(SpecialDayGroupList ** p_head)
{
	SpecialDayGroupList * p_next;
	SpecialDayGroupList * p_tmp = *p_head;
	while (p_tmp)
	{
		p_next = p_tmp->next;
#ifdef LIBICAL
		if (p_tmp->comp)
		{
			icalcomponent_free(p_tmp->comp);
		}
#endif
		free(p_tmp);
		p_tmp = p_next;
	}
	*p_head = NULL;
}

HT_API BOOL onvif_init_SpecialDayGroup()
{
	return TRUE;
}

#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT

HT_API void onvif_get_Receiver_token(ReceiverList * p_head, char * token, int size)
{
	ReceiverList * p_tmp = NULL;
	do
	{
		snprintf(token, size, "ReceiverToken_%u", ++g_onvif_idx.receiver_idx);
		p_tmp = onvif_find_Receiver(p_head, token);
	} while (p_tmp);
}

HT_API ReceiverList * onvif_add_Receiver(ReceiverList ** p_head)
{
	ReceiverList * p_tmp;
	ReceiverList * p_new = (ReceiverList *) malloc(sizeof(ReceiverList));
	if (NULL == p_new)
	{
		return NULL;
	}
	memset(p_new, 0, sizeof(ReceiverList));
	onvif_get_Receiver_token(*p_head, p_new->Receiver.Token, sizeof(p_new->Receiver.Token));
	p_tmp = *p_head;
	if (NULL == p_tmp)
	{
		*p_head = p_new;
	}
	else
	{
		while (p_tmp && p_tmp->next) p_tmp = p_tmp->next;

		p_tmp->next = p_new;
	}
	return p_new;
}

HT_API ReceiverList * onvif_find_Receiver(ReceiverList * p_head, const char * token)
{
	ReceiverList * p_tmp = p_head;  
	if (NULL == token)
	{
		return NULL;
	}
	while (p_tmp)
	{
		if (strcmp(token, p_tmp->Receiver.Token) == 0)
		{
			return p_tmp;
		}
		p_tmp = p_tmp->next;
	}
	return NULL;
}

HT_API void onvif_free_Receiver(ReceiverList ** p_head, ReceiverList * p_node)
{
	BOOL found = FALSE;
	ReceiverList * p_prev = NULL;
	ReceiverList * p_tmp = *p_head;	
	
	while (p_tmp)
	{
		if (p_tmp == p_node)
		{
			found = TRUE;
			break;
		}

		p_prev = p_tmp;
		p_tmp = p_tmp->next;
	}

	if (found)
	{
		if (NULL == p_prev)
		{
			*p_head = p_tmp->next;
		}
		else
		{
			p_prev->next = p_tmp->next;
		}
		
		free(p_tmp);
	}
}

HT_API void onvif_free_Receivers(ReceiverList ** p_head)
{
	ReceiverList * p_next;
	ReceiverList * p_tmp = *p_head;

	while (p_tmp)
	{
		p_next = p_tmp->next;

		free(p_tmp);
		p_tmp = p_next;
	}

	*p_head = NULL;
}

HT_API int onvif_get_Receiver_nums(ReceiverList * p_head)
{
	int nums = 0;
	ReceiverList * p_tmp = p_head;

	while (p_tmp)
	{
	    nums++;
		p_tmp = p_tmp->next;
	}

	return nums;
}

#endif // end of RECEIVER_SUPPORT

#ifdef IPFILTER_SUPPORT

HT_API BOOL onvif_is_ipaddr_filter_exist(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
    int i;
	for (i = 0; i < size; i++)
	{
		if (strcmp(p_item->Address, p_head[i].Address) == 0 && p_item->PrefixLength == p_head[i].PrefixLength)
		{
			return TRUE;
		}
	}

	return FALSE;	
}

HT_API onvif_PrefixedIPAddress * onvif_find_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
    int i;
	for (i = 0; i < size; i++)
	{
		if (strcmp(p_item->Address, p_head[i].Address) == 0 && p_item->PrefixLength == p_head[i].PrefixLength)
		{
			return &p_head[i];
		}
	}

	return NULL;
}

HT_API onvif_PrefixedIPAddress * onvif_get_idle_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size)
{
    int i;
    
	for (i = 0; i < size; i++)
	{
		if (p_head[i].Address[0] == '\0')
		{
			return &p_head[i];
		}
	}

	return NULL;
}

HT_API ONVIF_RET onvif_add_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item)
{
	onvif_PrefixedIPAddress * p_ipfilter;

	if (onvif_is_ipaddr_filter_exist(p_head, size, p_item) == TRUE)
	{
		return ONVIF_OK;
	}

	p_ipfilter = onvif_get_idle_ipaddr_filter(p_head, size);
	if (p_ipfilter)
	{
		p_ipfilter->PrefixLength = p_item->PrefixLength;
		strcpy(p_ipfilter->Address, p_item->Address);
		
		return ONVIF_OK;
	}

	return ONVIF_ERR_IPFilterListIsFull;
}

#endif // IPFILTER_SUPPORT

HT_API void onvif_init_MulticastConfiguration(onvif_MulticastConfiguration * p_cfg)
{
	p_cfg->Port = get_web_port();
	p_cfg->TTL = 1;
	p_cfg->AutoStart = FALSE;
	
	char IPv4Address[128] = {0};
	
	NET_IPV4 ip;
	ip.int32 = get_my_ipaddr();

	sprintf(IPv4Address, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]);
	//sprintf(IPv4Address, "http://%d.%d.%d.%d:%d/onvif/services", ip.str[0], ip.str[1], ip.str[2], ip.str[3], get_web_port());
	
	strcpy(p_cfg->IPv4Address, IPv4Address);
}

/*
 * Initialize the video source
 * 
 */
void onvif_init_VideoSource()
{
	log_print(HT_LOG_INFO, "onvif_init_VideoSource START\n");
	int w_max = 0, h_max = 0, max_frame = 0;
	int w = 0, h = 0, frame = 0;
	// char max_res_name[32] = {0};

	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[0].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &w, &h);
	frame =  gMediaCfg.videoConfig[0].videoEncode.encodeCfg[0].display_frameRate;
	log_print(HT_LOG_INFO, "video encode w:%d, h:%d, frame:%d\n", w, h, frame);
	
	// GetMaxVideoResEntryAll(0, max_res_name, &max_frame);
	// OnvifGetVideoSize(max_res_name, 0, &w_max, &h_max);
	// todo 
	w_max = 2560;
	h_max = 1440;	

	onvif_add_VideoSource(&g_onvif_cfg.v_src, w, h, frame, w_max, h_max, max_frame);
	if (g_stereo)
	{
		onvif_add_VideoSource(&g_onvif_cfg.v_src, w, h, frame, w_max, h_max, max_frame);
	}
	log_print(HT_LOG_INFO, "onvif_init_VideoSource OVER\n");
	return;
}

/*
 * Initialize the video source configuration options
 * 
 */
HT_API void onvif_init_VideoSourceConfigurationOptions(VideoSourceConfigurationList * p_item)
{
	// Specify the range can be configured to  the video source
	
	p_item->Options.BoundsRange.XRange.Min = 0;
	p_item->Options.BoundsRange.XRange.Max = p_item->Configuration.Bounds.width;

	p_item->Options.BoundsRange.YRange.Min = 0;
	p_item->Options.BoundsRange.YRange.Max = p_item->Configuration.Bounds.height;

	p_item->Options.BoundsRange.WidthRange.Min = 320;
	p_item->Options.BoundsRange.WidthRange.Max = p_item->Configuration.Bounds.width;

	p_item->Options.BoundsRange.HeightRange.Min = 240;
	p_item->Options.BoundsRange.HeightRange.Max = p_item->Configuration.Bounds.height;

	p_item->Options.sizeVideoSourceTokensAvailable = 1;
	strcpy(p_item->Options.VideoSourceTokensAvailable[0], p_item->Configuration.SourceToken);
	
	if (g_onvif_expand)
	{
		p_item->Options.ExtensionFlag = 1;
		
		p_item->Options.Extension.RotateFlag = 1;
		p_item->Options.Extension.Rotate.Reboot = 0;

		p_item->Options.Extension.Rotate.RotateMode_ON = 1;
		p_item->Options.Extension.Rotate.RotateMode_OFF = 1;
		p_item->Options.Extension.Rotate.sizeDegreeList = 3;
		
		p_item->Options.Extension.Rotate.DegreeList[0] = 90;
		p_item->Options.Extension.Rotate.DegreeList[1] = 180;
		p_item->Options.Extension.Rotate.DegreeList[2] = 270;
	}
}

void onvif_init_VideoSourceConfiguration_old()
{
	log_print(HT_LOG_INFO, "onvif_init_VideoSourceConfiguration_old()\n");
	VideoSourceConfigurationList * p_item;

	if (g_onvif_cfg.v_src_cfg)
	{
		return;
	}
	
	p_item = onvif_add_VideoSourceConfiguration(&g_onvif_cfg.v_src_cfg, 1280, 720);
	if (p_item)
	{
		VideoSourceList * p_v_src = onvif_find_VideoSource_by_size(g_onvif_cfg.v_src, 1280, 720);
		if (NULL == p_v_src)
		{
			p_v_src = onvif_add_VideoSource(&g_onvif_cfg.v_src, 1280, 720, 0, 0, 0, 0);
		}

		if (p_v_src)
		{
			strcpy(p_item->Configuration.SourceToken, p_v_src->VideoSource.token);
			strcpy(p_item->Options.VideoSourceTokensAvailable[0], p_v_src->VideoSource.token);
			p_item->Options.sizeVideoSourceTokensAvailable = 1;
		}
	}
	return;
}

void onvif_init_VideoSourceConfiguration()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_VideoSourceConfiguration() START\n");
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||||||||||||||||||||\n");
	VideoSourceConfigurationList * p_item;

	int w = 0, h = 0, frame = 0;
	OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[0].resolution.name, gMediaCfg.videoConfig[0].videoCapture.tvsystem, &w, &h);
	frame =  gMediaCfg.videoConfig[0].videoEncode.encodeCfg[0].display_frameRate;
	log_print(HT_LOG_INFO, "video encode w:%d, h:%d, frame:%d\n", w, h, frame);
	
	int w_max = 0, h_max = 0, max_frame = 0;
	char max_res_name[32] = {0};
	/* Get max resolution: replaces GetMaxVideoResEntryAll() */
	anj_sysmng_max_res_get(0, &w_max, &h_max);
	snprintf(max_res_name, 32, "%dx%d", w_max, h_max);
	max_frame = 25;
	OnvifGetVideoSize(max_res_name, 0, &w_max, &h_max);// get max resolution width and height
	
	p_item = onvif_add_VideoSourceConfiguration(&g_onvif_cfg.v_src_cfg, w_max, h_max);
	if (p_item)
	{
		VideoSourceList * p_v_src = onvif_find_VideoSource_by_size(g_onvif_cfg.v_src, w, h);
		if (NULL == p_v_src)
		{
			p_v_src = onvif_add_VideoSource(&g_onvif_cfg.v_src, w, h, frame, w_max, h_max, max_frame);
		}
		if (p_v_src)
		{
			strcpy(p_item->Configuration.SourceToken, p_v_src->VideoSource.token);
			strcpy(p_item->Options.VideoSourceTokensAvailable[0], p_v_src->VideoSource.token);
			p_item->Options.sizeVideoSourceTokensAvailable = 1;
		}
	}
	if (g_stereo)
	{
		p_item = onvif_add_VideoSourceConfiguration(&g_onvif_cfg.v_src_cfg, w_max, h_max);
		if (p_item)
		{
			VideoSourceList * p_v_src = onvif_find_VideoSource_by_size(g_onvif_cfg.v_src->next, w, h);
			if (NULL == p_v_src)
			{
				p_v_src = onvif_add_VideoSource(&g_onvif_cfg.v_src->next, w, h, frame, w_max, h_max, max_frame);
			}
			if (p_v_src)
			{
				strcpy(p_item->Configuration.SourceToken, p_v_src->VideoSource.token);
				strcpy(p_item->Options.VideoSourceTokensAvailable[0], p_v_src->VideoSource.token);
				p_item->Options.sizeVideoSourceTokensAvailable = 1;
			}
		}
	}
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_VideoSourceConfiguration() OVER\n");
	return;
}

#if 1
void onvif_init_VideoEncoderConfiguration()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_VideoEncoderConfiguration START\n");
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||||||||||||\n");
	VideoEncoder2ConfigurationList * p_item;
	int iIndex = 0;
	int width, height;

	if (g_stereo)
	{
		for( iIndex = 0; iIndex < 4 + (thirdstream_enable * 2); iIndex ++) /* iterate all encoder config slots */
		{
			p_item = onvif_add_VideoEncoder2Configuration(&g_onvif_cfg.v_enc_cfg, NULL);
			if (p_item)/* initialize current encoder config entry */
			{
				VideoEncode pVideoCfg;
				memcpy(&pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoEncode, sizeof(pVideoCfg));
				if (!strcmp(pVideoCfg.encodeCfg[iIndex % 3].encodeFormat.name, "H264"))
				{
					strcpy(p_item->Configuration.Encoding, "H264");
					p_item->Configuration.VideoEncoding = VideoEncoding_H264;
				}
				else if (!strcmp(pVideoCfg.encodeCfg[iIndex % 3].encodeFormat.name, "H265"))
				{
					strcpy(p_item->Configuration.Encoding, "H265");
					p_item->Configuration.VideoEncoding = VideoEncoding_H265;
				}
				else if (!strcmp(pVideoCfg.encodeCfg[iIndex % 3].encodeFormat.name, "MJPEG"))
				{
					strcpy(p_item->Configuration.Encoding, "JPEG");
					p_item->Configuration.VideoEncoding = VideoEncoding_JPEG;
				}
				if (1)
				{
					OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex % 3].resolution.name, 
					gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);// get current resolution width and height
					log_print(HT_LOG_INFO, "gMediaCfg.videoConfig[0].videoEncode.encodeCfg->bitRateControl:%s\n", gMediaCfg.videoConfig[0].videoEncode.encodeCfg->bitRateControl.name);
					p_item->Configuration.Resolution.Width = width;
					p_item->Configuration.Resolution.Height = height;
					VideoQualityEnum quality = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex % 3].bitRateQuality;
					if (quality == VIDEO_QUALITY_WORSER)
					{
						p_item->Configuration.Quality = 20;
					}
					else if (quality == VIDEO_QUALITY_WORSE)
					{
						p_item->Configuration.Quality = 40;
					}
					else if (quality == VIDEO_QUALITY_NORMAL)
					{
						p_item->Configuration.Quality = 60;
					}
					else if (quality == VIDEO_QUALITY_GOOD)
					{
						p_item->Configuration.Quality = 80;
					}
					else if (quality == VIDEO_QUALITY_BEST)
					{
						p_item->Configuration.Quality = 100;
					}
					else if (quality == VIDEO_QUALITY_CUSTOM)
					{
						p_item->Configuration.Quality = 50;
					}
					else
					{
						p_item->Configuration.Quality = 0;
					}
				}
				p_item->Configuration.RateControlFlag = 1;
				p_item->Configuration.RateControl.FrameRateLimit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex % 3].display_frameRate;//25;
				p_item->Configuration.RateControl.EncodingInterval = 1;
				p_item->Configuration.RateControl.BitrateLimit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex % 3].bitRate;//2048;
				p_item->Configuration.GovLengthFlag = 1;
				p_item->Configuration.GovLength = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex % 3].initQuant;
				p_item->Configuration.ProfileFlag = 1;
				strcpy(p_item->Configuration.Profile, "Main");
				p_item->Configuration.SessionTimeout = 10;
				p_item->Configuration.MulticastFlag = 1;
				onvif_init_MulticastConfiguration(&p_item->Configuration.Multicast);
			}
		}
	}
	else
	{
		for( iIndex = 0; iIndex < 2 + thirdstream_enable; iIndex ++)/* iterate all encoder config slots */
		{
			p_item = onvif_add_VideoEncoder2Configuration(&g_onvif_cfg.v_enc_cfg, NULL);
			if (p_item)/* initialize current encoder config entry */
			{
				VideoEncode pVideoCfg;
				memcpy(&pVideoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoEncode, sizeof(pVideoCfg));
				if (!strcmp(pVideoCfg.encodeCfg[iIndex].encodeFormat.name, "H264"))
				{
					strcpy(p_item->Configuration.Encoding, "H264");
					p_item->Configuration.VideoEncoding = VideoEncoding_H264;
				}
				else if (!strcmp(pVideoCfg.encodeCfg[iIndex].encodeFormat.name, "H265"))
				{
					strcpy(p_item->Configuration.Encoding, "H265");
					p_item->Configuration.VideoEncoding = VideoEncoding_H265;
				}
				else if (!strcmp(pVideoCfg.encodeCfg[iIndex].encodeFormat.name, "MJPEG"))
				{
					strcpy(p_item->Configuration.Encoding, "JPEG");
					p_item->Configuration.VideoEncoding = VideoEncoding_JPEG;
				}
				if (1)
				{
					OnvifGetVideoSize(gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].resolution.name, 
					gMediaCfg.videoConfig[0].videoCapture.tvsystem, &width, &height);// get current resolution width and height
					log_print(HT_LOG_INFO, "gMediaCfg.videoConfig[0].videoEncode.encodeCfg->bitRateControl:%s\n", gMediaCfg.videoConfig[0].videoEncode.encodeCfg->bitRateControl.name);
					p_item->Configuration.Resolution.Width = width;
					p_item->Configuration.Resolution.Height = height;
					VideoQualityEnum quality = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].bitRateQuality;
					if (quality == VIDEO_QUALITY_WORSER)
					{
						p_item->Configuration.Quality = 20;
					}
					else if (quality == VIDEO_QUALITY_WORSE)
					{
						p_item->Configuration.Quality = 40;
					}
					else if (quality == VIDEO_QUALITY_NORMAL)
					{
						p_item->Configuration.Quality = 60;
					}
					else if (quality == VIDEO_QUALITY_GOOD)
					{
						p_item->Configuration.Quality = 80;
					}
					else if (quality == VIDEO_QUALITY_BEST)
					{
						p_item->Configuration.Quality = 100;
					}
					else if (quality == VIDEO_QUALITY_CUSTOM)
					{
						p_item->Configuration.Quality = 50;
					}
					else
					{
						p_item->Configuration.Quality = 0;
					}
				}
				p_item->Configuration.RateControlFlag = 1;
				p_item->Configuration.RateControl.FrameRateLimit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].display_frameRate;//25;
				p_item->Configuration.RateControl.EncodingInterval = 1;
				p_item->Configuration.RateControl.BitrateLimit = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].bitRate;//2048;
				p_item->Configuration.GovLengthFlag = 1;
				p_item->Configuration.GovLength = gMediaCfg.videoConfig[0].videoEncode.encodeCfg[iIndex].initQuant;
				p_item->Configuration.ProfileFlag = 1;
				strcpy(p_item->Configuration.Profile, "Main");
				p_item->Configuration.SessionTimeout = 10;
				p_item->Configuration.MulticastFlag = 1;
				onvif_init_MulticastConfiguration(&p_item->Configuration.Multicast);
			}
		}
	}
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_VideoEncoderConfiguration OVER\n\n");
	return;
}

HT_API void onvif_init_VideoEncoder2ConfigurationOptions(onvif_VideoEncoder2Configuration * p_cfg, 
			onvif_VideoEncoder2ConfigurationOptions * p_option, const char * Encoding)
{
	RESOLUTION_ENTRY *pEntry = NULL;
	int stream_type = g_onvif_idx.v_enc_idx - 1; //0: main, 1: sub, 2: third
	int nrescount = anj_sysmng_video_res_array_get(&pEntry);
	int i, width, height, reslution;
	strcpy(p_option->Encoding, Encoding);
	
	if (strcasecmp(Encoding, "JPEG") == 0)
	{
		p_option->VideoEncoding = VideoEncoding_JPEG;
	}
	else if (strcasecmp(Encoding, "MP4V-ES") == 0)
	{
		p_option->VideoEncoding = VideoEncoding_MPEG4;

		p_option->GovLengthRangeFlag = 1;
		strcpy(p_option->GovLengthRange, "200 199 198 197 196 195 194 193 192 191 190 189 188 187 186 185 184 183 182 181 180 179 178 177 176 175 174 173 172 171 170 169 168 167 166 165 164 163 162 161 160 159 158 157 156 155 154 153 152 151 150 149 148 147 146 145 144 143 142 141 140 139 138 137 136 135 134 133 132 131 130 129 128 127 126 125 124 123 122 121 120 119 118 117 116 115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100 99 98 97 96 95 94 93 92 91 90 89 88 87 86 85 84 83 82 81 80 79 78 77 76 75 74 73 72 71 70 69 68 67 66 65 64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1");

		p_option->ProfilesSupportedFlag = 1;
		strcpy(p_option->ProfilesSupported, "Simple AdvancedSimple");
	}
	else if (strcasecmp(Encoding, "H264") == 0)
	{
		p_option->VideoEncoding = VideoEncoding_H264;

		p_option->GovLengthRangeFlag = 1;
		strcpy(p_option->GovLengthRange, "200 199 198 197 196 195 194 193 192 191 190 189 188 187 186 185 184 183 182 181 180 179 178 177 176 175 174 173 172 171 170 169 168 167 166 165 164 163 162 161 160 159 158 157 156 155 154 153 152 151 150 149 148 147 146 145 144 143 142 141 140 139 138 137 136 135 134 133 132 131 130 129 128 127 126 125 124 123 122 121 120 119 118 117 116 115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100 99 98 97 96 95 94 93 92 91 90 89 88 87 86 85 84 83 82 81 80 79 78 77 76 75 74 73 72 71 70 69 68 67 66 65 64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1");

		p_option->ProfilesSupportedFlag = 1;
		strcpy(p_option->ProfilesSupported, "Baseline Main High");
	}
	else if (strcasecmp(Encoding, "H265") == 0)
	{
		p_option->VideoEncoding = VideoEncoding_H264;
		p_option->GovLengthRangeFlag = 1;
		strcpy(p_option->GovLengthRange, "200 199 198 197 196 195 194 193 192 191 190 189 188 187 186 185 184 183 182 181 180 179 178 177 176 175 174 173 172 171 170 169 168 167 166 165 164 163 162 161 160 159 158 157 156 155 154 153 152 151 150 149 148 147 146 145 144 143 142 141 140 139 138 137 136 135 134 133 132 131 130 129 128 127 126 125 124 123 122 121 120 119 118 117 116 115 114 113 112 111 110 109 108 107 106 105 104 103 102 101 100 99 98 97 96 95 94 93 92 91 90 89 88 87 86 85 84 83 82 81 80 79 78 77 76 75 74 73 72 71 70 69 68 67 66 65 64 63 62 61 60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1");
		p_option->ProfilesSupportedFlag = 1;
		strcpy(p_option->ProfilesSupported, "Main Main10");
	}

	p_option->FrameRatesSupportedFlag = 1;
	sprintf(p_option->FrameRatesSupported, "60 59 58 57 56 55 54 53 52 51 50 49 48 47 46 45 44 43 42 41 40 39 38 37 36 35 34 33 32 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1");

	p_option->ConstantBitRateSupported = 1;

	p_option->MaxAnchorFrameDistanceFlag = 0;
	p_option->MaxAnchorFrameDistance = 0;

	p_option->GuaranteedFrameRateSupported = 0;

	p_option->QualityRange.Min = 0;
	p_option->QualityRange.Max = 100;

	p_option->BitrateRange.Min = 64;
	p_option->BitrateRange.Max = 12288;

	reslution = 0;
	if (strcasecmp(Encoding, "JPEG") == 0)
	{
		for( i = 0; i < nrescount; i++)
		{
			if (strncmp(pEntry[i].codec_name, "MJPEG", strlen(pEntry[i].codec_name)) == 0 && pEntry[i].stream_type == stream_type)
			{
				OnvifGetVideoSize(pEntry[i].res_name, stream_type, &width, &height);
				p_option->ResolutionsAvailable[reslution].Width = width;
				p_option->ResolutionsAvailable[reslution].Height = height;
				reslution ++ ;
				//log_print(HT_LOG_INFO, "Encoding:%s, stream_type:%d, width:%d, height:%d\n", "MJPEG", stream_type, width, height);
			}
		}
	}
	else
	{
		for( i = 0; i < nrescount; i++)
		{
			if (strncmp(pEntry[i].codec_name, Encoding, strlen(pEntry[i].codec_name)) == 0 && pEntry[i].stream_type == stream_type)
			{
				OnvifGetVideoSize(pEntry[i].res_name, stream_type, &width, &height);
				p_option->ResolutionsAvailable[reslution].Width = width;
				p_option->ResolutionsAvailable[reslution].Height = height;
				reslution ++ ;
				//log_print(HT_LOG_INFO, "Encoding:%s, stream_type:%d, width:%d, height:%d\n", Encoding, stream_type, width, height);
			}
		}
	}
	return;
}

HT_API void onvif_init_VideoEncoderConfigurationOptions(VideoEncoder2ConfigurationList * p_item)
{
	log_print(HT_LOG_INFO, "onvif_init_VideoEncoderConfigurationOptions token:%s START\n", p_item->Configuration.token);
	int i, width, height, reslution;
	int stream_type = -1;
	int has_ext_option = 0;
	if (g_stereo)
	{
		if (thirdstream_enable)
		{
			if (g_onvif_idx.v_enc_idx == 1 || g_onvif_idx.v_enc_idx == 4)
				stream_type = 0;
			else if (g_onvif_idx.v_enc_idx == 2 || g_onvif_idx.v_enc_idx == 4)
				stream_type = 1;
			else if (g_onvif_idx.v_enc_idx == 3 || g_onvif_idx.v_enc_idx == 5)
				stream_type = 2;
		}
		else
		{
			if (g_onvif_idx.v_enc_idx == 1 || g_onvif_idx.v_enc_idx == 3)
				stream_type = 0;
			else if (g_onvif_idx.v_enc_idx == 2 || g_onvif_idx.v_enc_idx == 4)
				stream_type = 1;
		}
	}
	else
		stream_type = g_onvif_idx.v_enc_idx - 1; //0: main, 1: sub, 2: third
		
	VideoEncoder2ConfigurationOptionsList * p_option;
	
	RESOLUTION_ENTRY *pEntry = NULL;
	int nrescount = anj_sysmng_video_res_array_get(&pEntry);
	
	p_item->Options.QualityRange.Min = 0;
	p_item->Options.QualityRange.Max = 100;
	
	for( i = 0; i < nrescount; i++)
	{
		if(strncmp(pEntry[i].codec_name, "H264", strlen("H264")) == 0 && pEntry[i].stream_type == stream_type)
			p_item->Options.H264Flag = EXIST;
		else if(strncmp(pEntry[i].codec_name, "H265", strlen("H265")) == 0  && pEntry[i].stream_type == stream_type)
			p_item->Options.H265Flag = EXIST;
		else if(strncmp(pEntry[i].codec_name, "MJPEG", strlen("MJPEG")) == 0  && pEntry[i].stream_type == stream_type)
			p_item->Options.JPEGFlag = EXIST;
	}
	
	if (p_item->Options.H264Flag == EXIST)
	{
		log_print(HT_LOG_INFO, "(p_item->Options.H264Flag == EXIST)\n");
		p_option = onvif_add_VideoEncoder2ConfigurationOptions(&p_item->Options2);
		onvif_init_VideoEncoder2ConfigurationOptions(&p_item->Configuration, &p_option->Options, "H264");
		has_ext_option = 1;
		
		p_item->Options.H264.H264Profile_Baseline = 1;
		p_item->Options.H264.H264Profile_Main = 1;
		p_item->Options.H264.H264Profile_High = 1;
		
		p_item->Options.H264.GovLengthRange.Min = 1;
		p_item->Options.H264.GovLengthRange.Max = 200;
		p_item->Options.H264.FrameRateRange.Min = 1;
		p_item->Options.H264.FrameRateRange.Max = 60;
		p_item->Options.H264.EncodingIntervalRange.Min = 1;
		p_item->Options.H264.EncodingIntervalRange.Max = 1;
		reslution = 0;
		for( i = 0; i < nrescount; i++)
		{
			if (strncmp(pEntry[i].codec_name, "H264", strlen(pEntry[i].codec_name)) == 0 && pEntry[i].stream_type == stream_type)
			{
				OnvifGetVideoSize(pEntry[i].res_name, stream_type, &width, &height);
				p_item->Options.H264.ResolutionsAvailable[reslution].Width = width;
				p_item->Options.H264.ResolutionsAvailable[reslution].Height = height;
				reslution ++;
				log_print(HT_LOG_INFO, "pEntry[%d].codec_name:%s, stream_type:%d, width:%d, height:%d\n", reslution,  pEntry[i].codec_name, stream_type, width, height);
			}
		}

		p_item->Options.Extension.H264Flag = 1;
		memcpy(&p_item->Options.Extension.H264.H264Options,
			&p_item->Options.H264,
			sizeof(onvif_H264Options));
		p_item->Options.Extension.H264.BitrateRange.Min = 64;
		p_item->Options.Extension.H264.BitrateRange.Max = 12288;
	}

	if (p_item->Options.H265Flag == EXIST)
	{
		log_print(HT_LOG_INFO, "(p_item->Options.H265Flag == EXIST)\n");
		p_option = onvif_add_VideoEncoder2ConfigurationOptions(&p_item->Options2);
		onvif_init_VideoEncoder2ConfigurationOptions(&p_item->Configuration, &p_option->Options, "H265");
		p_item->Options.H265.H265Profile_Baseline = 1;
		p_item->Options.H265.H265Profile_Main = 1;
		p_item->Options.H265.H265Profile_High = 1;
		p_item->Options.H265.GovLengthRange.Min = 1;
		p_item->Options.H265.GovLengthRange.Max = 200;
		p_item->Options.H265.FrameRateRange.Min = 1;
		p_item->Options.H265.FrameRateRange.Max = 60;
		p_item->Options.H265.EncodingIntervalRange.Min = 1;
		p_item->Options.H265.EncodingIntervalRange.Max = 1;
		reslution = 0;
		for( i = 0; i < nrescount; i++)
		{
			if (strncmp(pEntry[i].codec_name, "H265", strlen(pEntry[i].codec_name)) == 0 && pEntry[i].stream_type == stream_type)
			{
				OnvifGetVideoSize(pEntry[i].res_name, stream_type, &width, &height);
				p_item->Options.H265.ResolutionsAvailable[reslution].Width = width;
				p_item->Options.H265.ResolutionsAvailable[reslution].Height = height;
				reslution ++;
				log_print(HT_LOG_INFO, "pEntry[%d].codec_name:%s, stream_type:%d, width:%d, height:%d\n", reslution,  pEntry[i].codec_name, stream_type, width, height);
			}
		}
	}
	if (p_item->Options.JPEGFlag == EXIST)
	{
		log_print(HT_LOG_INFO, "(p_item->Options.JPEGFlag == EXIST)\n");
		p_option = onvif_add_VideoEncoder2ConfigurationOptions(&p_item->Options2);
		onvif_init_VideoEncoder2ConfigurationOptions(&p_item->Configuration, &p_option->Options, "JPEG");
		has_ext_option = 1;
		p_item->Options.JPEG.FrameRateRange.Min = 1;
		p_item->Options.JPEG.FrameRateRange.Max = 30;
		p_item->Options.JPEG.EncodingIntervalRange.Min = 1;
		p_item->Options.JPEG.EncodingIntervalRange.Max = 60;
		reslution = 0;
		for( i = 0; i < nrescount; i++)
		{
			if (strncmp(pEntry[i].codec_name, "MJPEG", strlen(pEntry[i].codec_name)) == 0 && pEntry[i].stream_type == stream_type)
			{
				OnvifGetVideoSize(pEntry[i].res_name, stream_type, &width, &height);
				p_item->Options.JPEG.ResolutionsAvailable[reslution].Width = width;
				p_item->Options.JPEG.ResolutionsAvailable[reslution].Height = height;
				reslution ++;
				log_print(HT_LOG_INFO, "pEntry[%d].codec_name:%s, stream_type:%d, width:%d, height:%d\n", reslution, pEntry[i].codec_name, stream_type, width, height);
			}
		}

		p_item->Options.Extension.JPEGFlag = 1;
		memcpy(&p_item->Options.Extension.JPEG.JpegOptions,
			&p_item->Options.JPEG,
			sizeof(onvif_JpegOptions));
		p_item->Options.Extension.JPEG.BitrateRange.Min = 64;
		p_item->Options.Extension.JPEG.BitrateRange.Max = 12288;
	}

	p_item->Options.ExtensionFlag = has_ext_option ? 1 : 0;
	log_print(HT_LOG_INFO, "onvif_init_VideoEncoderConfigurationOptions OVER\n");
	return;
}

#endif
void onvif_init_MetadataConfiguration()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_MetadataConfiguration START\n");
	MetadataConfigurationList * p_node;
	
	if (g_onvif_cfg.metadata_cfg)
	{
		return;
	}

	p_node = onvif_add_MetadataConfiguration(&g_onvif_cfg.metadata_cfg);
	if (NULL == p_node)
	{
		return;
	}

	p_node->Configuration.AnalyticsFlag = 1;
	p_node->Configuration.Analytics = TRUE;

	p_node->Configuration.SessionTimeout = 60;
	p_node->Configuration.PTZStatusFlag = 1;
	p_node->Configuration.PTZStatus.Status = 1;
	p_node->Configuration.PTZStatus.Position = 1;

	onvif_init_MulticastConfiguration(&p_node->Configuration.Multicast);
	//log_print(HT_LOG_INFO, "\t onvif_init_MetadataConfiguration OVER\n\n");
	return;
}

void onvif_init_MetadataConfigurationOptions()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_MetadataConfigurationOptions START\n");
	onvif_MetadataConfigurationOptions * p_opt = &g_onvif_cfg.MetadataConfigurationOptions;

	p_opt->PTZStatusFilterOptions.PanTiltPositionSupported = FALSE;
	p_opt->PTZStatusFilterOptions.ZoomPositionSupported = FALSE;
	p_opt->PTZStatusFilterOptions.PanTiltStatusSupported = TRUE;
	p_opt->PTZStatusFilterOptions.ZoomStatusSupported = TRUE;
	//log_print(HT_LOG_INFO, "\t onvif_init_MetadataConfigurationOptions OVER\n\n");
	return;
}

void onvif_renew_OSDConfigurations(OSDConfigurationList         *OSDs)
{
	log_print(HT_LOG_INFO, "\t onvif_renew_OSDConfigurations START\n");
	log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||||\n");
	OSDConfigurationList * p_osd = OSDs;
	
	VideoOverlay overlay;
	memcpy(&overlay, &((MediaConfig *)getMediaConfig())->videoConfig[0].overlay, sizeof(overlay));
	
	if (!strcmp(p_osd->OSD.token, "OSDConfigurationToken_1"))
	{
		if (overlay.titleOverlay.posX == 0 && overlay.titleOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		else if (overlay.titleOverlay.posX == 1 && overlay.titleOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperRight;
		else if (overlay.titleOverlay.posX == 0 && overlay.titleOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerLeft;
		else if (overlay.titleOverlay.posX == 1 && overlay.titleOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerRight;
		else
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		
		memset(p_osd->OSD.TextString.PlainText, 0, sizeof(p_osd->OSD.TextString.PlainText));
		strcpy(p_osd->OSD.TextString.PlainText, overlay.titleOverlay.title_utf8);
	}
	if (!strcmp(p_osd->OSD.token, "OSDConfigurationToken_2"))
	{
		if (overlay.timeOverlay.posX == 0 && overlay.timeOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		else if (overlay.timeOverlay.posX == 1 && overlay.timeOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperRight;
		else if (overlay.timeOverlay.posX == 0 && overlay.timeOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerLeft;
		else if (overlay.timeOverlay.posX == 1 && overlay.timeOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerRight;
		else
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;

		memset(p_osd->OSD.TextString.DateFormat, 0, sizeof(p_osd->OSD.TextString.DateFormat));
		if(strstr(overlay.timeOverlay.timeFormat.format, "yy-mm-dd") || strstr(overlay.timeOverlay.timeFormat.format, "yyyy-mm-dd"))
			strcpy(p_osd->OSD.TextString.DateFormat, "yyyy-MM-dd");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "yyyy/mm/dd") || strstr(overlay.timeOverlay.timeFormat.format, "yy/mm/dd"))
			strcpy(p_osd->OSD.TextString.DateFormat, "yyyy/MM/dd");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "dd-mm-yyyy") || strstr(overlay.timeOverlay.timeFormat.format, "dd/mm/yyyy"))
			strcpy(p_osd->OSD.TextString.DateFormat, "dd/MM/yyyy");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "mm-dd-yyyy") || strstr(overlay.timeOverlay.timeFormat.format, "mm/dd/yyyy"))
			strcpy(p_osd->OSD.TextString.DateFormat, "MM/dd/yyyy");
		
	}
	log_print(HT_LOG_INFO, "onvif_renew_OSDConfigurations OVER\n");
	return;
}

void onvif_init_OSDConfigurations()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_OSDConfigurations START\n");
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||||\n");
	OSDConfigurationList * p_osd;

	if (g_onvif_cfg.OSDs)
	{
		return;
	}
	
	VideoOverlay overlay;
	memcpy(&overlay, &((MediaConfig *)getMediaConfig())->videoConfig[0].overlay, sizeof(overlay));
	
	p_osd = onvif_add_OSDConfiguration(&g_onvif_cfg.OSDs);
	if (p_osd)
	{
		if (g_onvif_cfg.v_src_cfg)
		{
			strcpy(p_osd->OSD.VideoSourceConfigurationToken, g_onvif_cfg.v_src_cfg->Configuration.token);
		}
		
		p_osd->OSD.Type = OSDType_Text;
		
		if (overlay.titleOverlay.posX == 0 && overlay.titleOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		else if (overlay.titleOverlay.posX == 1 && overlay.titleOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperRight;
		else if (overlay.titleOverlay.posX == 0 && overlay.titleOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerLeft;
		else if (overlay.titleOverlay.posX == 1 && overlay.titleOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerRight;
		else
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		
		p_osd->OSD.TextStringFlag = 1;
		p_osd->OSD.TextString.Type = OSDTextType_Plain;
		p_osd->OSD.TextString.PlainTextFlag = 1;
		
		
		strcpy(p_osd->OSD.TextString.PlainText, overlay.titleOverlay.title_utf8);
	}
	
	p_osd = onvif_add_OSDConfiguration(&g_onvif_cfg.OSDs);
	if (p_osd)
	{
		if (g_onvif_cfg.v_src_cfg)
		{
			strcpy(p_osd->OSD.VideoSourceConfigurationToken, g_onvif_cfg.v_src_cfg->Configuration.token);
		}

		p_osd->OSD.Type = OSDType_Text;
		
		if (overlay.timeOverlay.posX == 0 && overlay.timeOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		else if (overlay.timeOverlay.posX == 1 && overlay.timeOverlay.posY == 0)
			p_osd->OSD.Position.Type = OSDPosType_UpperRight;
		else if (overlay.timeOverlay.posX == 0 && overlay.timeOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerLeft;
		else if (overlay.timeOverlay.posX == 1 && overlay.timeOverlay.posY == 1)
			p_osd->OSD.Position.Type = OSDPosType_LowerRight;
		else
			p_osd->OSD.Position.Type = OSDPosType_UpperLeft;
		
		p_osd->OSD.TextStringFlag = 1;
		p_osd->OSD.TextString.Type = OSDTextType_DateAndTime;
		p_osd->OSD.TextString.DateFormatFlag = 1;
		
		if(strstr(overlay.timeOverlay.timeFormat.format, "yy-mm-dd") || strstr(overlay.timeOverlay.timeFormat.format, "yyyy-mm-dd"))
			strcpy(p_osd->OSD.TextString.DateFormat, "yyyy-MM-dd");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "yyyy/mm/dd") || strstr(overlay.timeOverlay.timeFormat.format, "yy/mm/dd"))
			strcpy(p_osd->OSD.TextString.DateFormat, "yyyy/MM/dd");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "dd-mm-yyyy") || strstr(overlay.timeOverlay.timeFormat.format, "dd/mm/yyyy"))
			strcpy(p_osd->OSD.TextString.DateFormat, "dd/MM/yyyy");
		else if(strstr(overlay.timeOverlay.timeFormat.format, "mm-dd-yyyy") || strstr(overlay.timeOverlay.timeFormat.format, "mm/dd/yyyy"))
			strcpy(p_osd->OSD.TextString.DateFormat, "MM/dd/yyyy");
		
		p_osd->OSD.TextString.TimeFormatFlag = 1;
		strcpy(p_osd->OSD.TextString.TimeFormat, "HH:mm:ss");
		
	}
	
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_OSDConfigurations OVER\n\n");
	return;
}

void onvif_init_OSDConfigurationOptions()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_OSDConfigurationOptions START\n");
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||||||||||||||||\n");
	onvif_OSDConfigurationOptions * p_opt = &g_onvif_cfg.OSDConfigurationOptions;

	p_opt->OSDType_Text = 1;
	p_opt->OSDType_Image = 0;
	p_opt->OSDType_Extended = 0;
	p_opt->OSDPosType_UpperLeft = 1;
	p_opt->OSDPosType_UpperRight = 1;
	p_opt->OSDPosType_LowerLeft = 1;
	p_opt->OSDPosType_LowerRight = 1;
	p_opt->OSDPosType_Custom = 1;
	p_opt->TextOptionFlag = 1;
	p_opt->ImageOptionFlag = 0;
	
	p_opt->MaximumNumberOfOSDs.ImageFlag = 0;
	p_opt->MaximumNumberOfOSDs.PlainTextFlag = 1;
	p_opt->MaximumNumberOfOSDs.DateFlag = 1;
	p_opt->MaximumNumberOfOSDs.TimeFlag = 1;
	p_opt->MaximumNumberOfOSDs.DateAndTimeFlag = 1;

	p_opt->MaximumNumberOfOSDs.Total = 2;
	p_opt->MaximumNumberOfOSDs.Image = 0;
	p_opt->MaximumNumberOfOSDs.PlainText = 0;
	p_opt->MaximumNumberOfOSDs.Date = 0;
	p_opt->MaximumNumberOfOSDs.Time = 0;
	p_opt->MaximumNumberOfOSDs.DateAndTime = 1;
	
	p_opt->TextOption.OSDTextType_Plain = 1;
	p_opt->TextOption.OSDTextType_Date = 0;
	p_opt->TextOption.OSDTextType_Time = 0;
	p_opt->TextOption.OSDTextType_DateAndTime = 1;
	p_opt->TextOption.FontSizeRangeFlag = 0;
	p_opt->TextOption.FontColorFlag = 0;
	p_opt->TextOption.BackgroundColorFlag = 0;

	//p_opt->TextOption.FontSizeRange.Min = 16;
	//p_opt->TextOption.FontSizeRange.Max = 64;

	p_opt->TextOption.DateFormatSize = 7;
	strcpy(p_opt->TextOption.DateFormat[0], "MM/dd/yyyy");
	strcpy(p_opt->TextOption.DateFormat[1], "dd/MM/yyyy");
	strcpy(p_opt->TextOption.DateFormat[2], "yyyy/MM/dd");
	strcpy(p_opt->TextOption.DateFormat[3], "yyyy-MM-dd");
	strcpy(p_opt->TextOption.DateFormat[4], "yy/MM/dd");
	strcpy(p_opt->TextOption.DateFormat[5], "dd-MM-yyyy");
	strcpy(p_opt->TextOption.DateFormat[6], "MM-dd-yyyy");

	p_opt->TextOption.TimeFormatSize = 1;
	strcpy(p_opt->TextOption.TimeFormat[0], "HH:mm:ss");
	//log_print(HT_LOG_INFO, "\t||||||||||||||||||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\t onvif_init_OSDConfigurationOptions OVER\n\n");
	return;
}

void onvif_init_profile()
{
	//log_print(HT_LOG_INFO, "\t onvif_init_profile() START\n");
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||||\n");
	ONVIF_PROFILE * profile;

	if (g_onvif_cfg.profiles)
	{
		return;
	}
	
	int iIndex = 0;
	int preset_i = 0;
	
	if (g_stereo)
	{
		for( iIndex = 0; iIndex < 4 + (thirdstream_enable*2); iIndex ++)
		{
			profile = onvif_add_profile(&g_onvif_cfg.profiles, TRUE);
			if (profile)
			{
				if(thirdstream_enable)
				{
					if (iIndex == 0 || iIndex == 1 || iIndex == 2)
					{
						if (g_onvif_cfg.v_src_cfg)
						{
							profile->v_src_cfg = (g_onvif_cfg.v_src_cfg);
							profile->v_src_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 3 || iIndex == 4 || iIndex == 5)
					{
						if (g_onvif_cfg.v_src_cfg->next)
						{
							profile->v_src_cfg = (g_onvif_cfg.v_src_cfg->next);
							profile->v_src_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 0)
					{
						if (g_onvif_cfg.v_enc_cfg)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 1)
					{
						if (g_onvif_cfg.v_enc_cfg->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 2)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 3)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 4)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 5)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next->next->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next->next->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
				}
				else
				{
					if (iIndex == 0 || iIndex == 1)
					{
						if (g_onvif_cfg.v_src_cfg)
						{
							profile->v_src_cfg = (g_onvif_cfg.v_src_cfg);
							profile->v_src_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 3 || iIndex == 2)
					{
						if (g_onvif_cfg.v_src_cfg->next)
						{
							profile->v_src_cfg = (g_onvif_cfg.v_src_cfg->next);
							profile->v_src_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 0)
					{
						if (g_onvif_cfg.v_enc_cfg)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 1)
					{
						if (g_onvif_cfg.v_enc_cfg->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 2)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
					if (iIndex == 3)
					{
						if (g_onvif_cfg.v_enc_cfg->next->next->next)
						{
							profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next->next);
							profile->v_enc_cfg->Configuration.UseCount++;
						}
					}
				}
#ifdef AUDIO_SUPPORT
				if (g_onvif_cfg.a_src_cfg)
				{
					profile->a_src_cfg = (g_onvif_cfg.a_src_cfg);
					profile->a_src_cfg->Configuration.UseCount++;
				}
				if (g_onvif_cfg.a_enc_cfg)
				{
					profile->a_enc_cfg = (g_onvif_cfg.a_enc_cfg);
					profile->a_enc_cfg->Configuration.UseCount++;
				}
#endif
				for (preset_i = 0; preset_i < 255; preset_i ++)
				{
#ifdef PTZ_SUPPORT
					//log_print(HT_LOG_INFO, "onvif_add_PTZPreset:%d\n", preset_i);
					onvif_add_PTZPreset(&profile->presets);
#endif
				}
			}
		}
	}
	else
	{
		for( iIndex = 0; iIndex < 2 + thirdstream_enable; iIndex ++)
		{
			profile = onvif_add_profile(&g_onvif_cfg.profiles, FALSE);
			if (profile)
			{
				if (g_onvif_cfg.v_src_cfg)
				{
					profile->v_src_cfg = (g_onvif_cfg.v_src_cfg);
					profile->v_src_cfg->Configuration.UseCount++;
				}
				if (iIndex == 0)
				{
					if (g_onvif_cfg.v_enc_cfg)
					{
						profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg);
						profile->v_enc_cfg->Configuration.UseCount++;
					}
				}
				if (iIndex == 1)
				{
					if (g_onvif_cfg.v_enc_cfg->next)
					{
						profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next);
						profile->v_enc_cfg->Configuration.UseCount++;
					}
				}
				if (iIndex == 2)
				{
					if (g_onvif_cfg.v_enc_cfg->next->next)
					{
						profile->v_enc_cfg = (g_onvif_cfg.v_enc_cfg->next->next);
						profile->v_enc_cfg->Configuration.UseCount++;
					}
				}
#ifdef AUDIO_SUPPORT
				if (g_onvif_cfg.a_src_cfg)
				{
					profile->a_src_cfg = (g_onvif_cfg.a_src_cfg);
					profile->a_src_cfg->Configuration.UseCount++;
				}
				if (g_onvif_cfg.a_enc_cfg)
				{
					profile->a_enc_cfg = (g_onvif_cfg.a_enc_cfg);
					profile->a_enc_cfg->Configuration.UseCount++;
				}
#endif
				for (preset_i = 0; preset_i < 255; preset_i ++)
				{
#ifdef PTZ_SUPPORT
					//log_print(HT_LOG_INFO, "onvif_add_PTZPreset:%d\n", preset_i);
					onvif_add_PTZPreset(&profile->presets);
#endif
				}
			}
		}
	}
	//log_print(HT_LOG_INFO, "\t|||||||||||||||||||||||||\n");
	//log_print(HT_LOG_INFO, "\tonvif_init_profile() OVER\n");
	return;
}

void onvif_init_profile_old()
{
	ONVIF_PROFILE * p_item;

	if (g_onvif_cfg.profiles)
	{
		return;
	}

	p_item = onvif_add_profile(&g_onvif_cfg.profiles, TRUE);
	if (p_item)
	{
		p_item->v_src_cfg = g_onvif_cfg.v_src_cfg;
		if (p_item->v_src_cfg)
		{
			p_item->v_src_cfg->Configuration.UseCount++;
		}

		p_item->v_enc_cfg = g_onvif_cfg.v_enc_cfg;
		if (p_item->v_enc_cfg)
		{
			p_item->v_enc_cfg->Configuration.UseCount++;
		}

#ifdef AUDIO_SUPPORT
		p_item->a_src_cfg = g_onvif_cfg.a_src_cfg;
		if (p_item->a_src_cfg)
		{
			p_item->a_src_cfg->Configuration.UseCount++;
		}

		p_item->a_enc_cfg = g_onvif_cfg.a_enc_cfg;
		if (p_item->a_enc_cfg)
		{
			p_item->a_enc_cfg->Configuration.UseCount++;
		}
#endif
	}

	p_item = onvif_add_profile(&g_onvif_cfg.profiles, TRUE);
	if (p_item)
	{
		p_item->v_src_cfg = g_onvif_cfg.v_src_cfg;
		if (p_item->v_src_cfg)
		{
			p_item->v_src_cfg->Configuration.UseCount++;
		}

		if (g_onvif_cfg.v_enc_cfg)
		{
			p_item->v_enc_cfg = g_onvif_cfg.v_enc_cfg->next;
		}
		else
		{
			p_item->v_enc_cfg = g_onvif_cfg.v_enc_cfg;
		}

		if (p_item->v_enc_cfg)
		{
			p_item->v_enc_cfg->Configuration.UseCount++;
		}

#ifdef AUDIO_SUPPORT
		p_item->a_src_cfg = g_onvif_cfg.a_src_cfg;
		if (p_item->a_src_cfg)
		{
			p_item->a_src_cfg->Configuration.UseCount++;
		}

		p_item->a_enc_cfg = g_onvif_cfg.a_enc_cfg;
		if (p_item->a_enc_cfg)
		{
			p_item->a_enc_cfg->Configuration.UseCount++;
		}
#endif
	}
}

#ifdef IMAGE_SUPPORT

HT_API void onvif_refresh_ImagingSettings(onvif_ImagingSettings * p_item)
{
	// init image setting	
	// by system
	
	VideoCaptureCfg videoCfg;
	memcpy(&videoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(videoCfg));
	VideoCaptureCfg *capCfg = &videoCfg;
	log_print(HT_LOG_INFO, "capCfg->backlight:%d\n", capCfg->backlight);
	if(capCfg->backlight > 0)
	{
		p_item->BacklightCompensation.Mode = BacklightCompensationMode_ON;
		p_item->BacklightCompensationFlag = 1;
		p_item->BacklightCompensation.LevelFlag = 1;
		p_item->BacklightCompensation.Level = capCfg->backlight;
	}
	else
	{
		p_item->BacklightCompensation.Mode = BacklightCompensationMode_OFF;
		p_item->BacklightCompensationFlag = 1;
		p_item->BacklightCompensation.LevelFlag = 1;
		p_item->BacklightCompensation.Level = 0;
	}
	if (1)
	{
		p_item->BrightnessFlag = 1;
		p_item->Brightness = capCfg->brightness;
		p_item->ColorSaturationFlag = 1;
		p_item->ColorSaturation = capCfg->saturation;
		p_item->ContrastFlag = 1;
		p_item->Contrast = capCfg->contrast;
		
	}
	p_item->SharpnessFlag = 1;
	p_item->Sharpness = capCfg->sharpness;
	if (1)
	{
		p_item->ExposureFlag = 1;
		p_item->Exposure.Mode = capCfg->shutterSetting.shutter_mode_day == ExposureMode_AUTO ? ExposureMode_AUTO : ExposureMode_MANUAL;
		
		p_item->Exposure.PriorityFlag = 0;
		p_item->Exposure.Priority = ExposurePriority_LowNoise;

		p_item->Exposure.Window.bottom = 0;
		p_item->Exposure.Window.top = 0;
		p_item->Exposure.Window.right = 0;
		p_item->Exposure.Window.left = 0;
		
		p_item->Exposure.MinExposureTimeFlag = 1;
		p_item->Exposure.MinExposureTime = 100;
		
		p_item->Exposure.MaxExposureTimeFlag = 1;
		p_item->Exposure.MaxExposureTime = 100000;
		
		p_item->Exposure.MinGainFlag = 0;
		p_item->Exposure.MinGain = 0;
		
		p_item->Exposure.MaxGainFlag = 0;
		p_item->Exposure.MaxGain = 100;
		
		p_item->Exposure.MinIrisFlag = 0;
		p_item->Exposure.MinIris = 0;
		
		p_item->Exposure.MaxIrisFlag = 0;
		p_item->Exposure.MaxIris = 0;
		
		p_item->Exposure.ExposureTimeFlag = 1;
		p_item->Exposure.ExposureTime = 1000000/capCfg->shutterSetting.shutter_speed_day;

		p_item->Exposure.GainFlag = 0;
		p_item->Exposure.Gain = 0;
		
		p_item->Exposure.IrisFlag = 0;
		p_item->Exposure.Iris = 0;
	}
	if (1)
	{
		p_item->FocusFlag = 1;
		p_item->Focus.AutoFocusMode = AutoFocusMode_MANUAL;

		p_item->Focus.DefaultSpeedFlag = 1;
		p_item->Focus.DefaultSpeed = 1;

		p_item->Focus.NearLimitFlag = 1;
		p_item->Focus.NearLimit = 1;
		
		p_item->Focus.FarLimitFlag = 1;
		p_item->Focus.FarLimit = 10;
	}
	if (1)
	{
		p_item->IrCutFilterFlag = 1;
		if(capCfg->ircut_mode == IRCUT_Mode_Passive)
			p_item->IrCutFilter = IrCutFilterMode_AUTO;
		else if(capCfg->ircut_mode == IRCUT_Mode_Manual)
		{
			if( capCfg->ircut_keepcolor == 0 )
				p_item->IrCutFilter = IrCutFilterMode_OFF;//off
			else 
				p_item->IrCutFilter = IrCutFilterMode_ON;//on
		}
		else
			p_item->IrCutFilter = IrCutFilterMode_AUTO;
	}
	
	p_item->WideDynamicRangeFlag = 1;
	p_item->WideDynamicRange.Mode = capCfg->wdr_mode == 0 ? WideDynamicMode_OFF : WideDynamicMode_ON;
	
	p_item->WideDynamicRange.LevelFlag = 1;
#if !PLATFORM_DM365
		p_item->WideDynamicRange.Level = capCfg->wdr_value;
#else
		p_item->WideDynamicRange.Level = 0;
#endif
	if(p_item->WideDynamicRange.Level > 255)
		p_item->WideDynamicRange.Level = 255;
	else if(p_item->WideDynamicRange.Level < 0)
		p_item->WideDynamicRange.Level = 0;
	
	p_item->WhiteBalanceFlag = 1;
	if((capCfg->whitebalance >> 24) & 0xff)
		p_item->WhiteBalance.Mode = WhiteBalanceMode_MANUAL;
	else
		p_item->WhiteBalance.Mode = WhiteBalanceMode_AUTO;
	
	p_item->WhiteBalance.CbGainFlag = 1;
	p_item->WhiteBalance.CbGain = (capCfg->whitebalance) & 0xff;
	
	p_item->WhiteBalance.CrGainFlag = 1;
	p_item->WhiteBalance.CrGain = (capCfg->whitebalance>>16) & 0xff;;

	return;
}

HT_API void onvif_init_ImagingSettings(VideoSourceList * p_vsrc, onvif_ImagingSettings * p_item)
{
	// init image setting	
	// by system
	
	VideoCaptureCfg videoCfg;
	memcpy(&videoCfg, &((MediaConfig *)getMediaConfig())->videoConfig[0].videoCapture, sizeof(videoCfg));
	VideoCaptureCfg *capCfg = &videoCfg;
	log_print(HT_LOG_INFO, "capCfg->backlight:%d\n", capCfg->backlight);
	if(capCfg->backlight > 0)
	{
		p_item->BacklightCompensation.Mode = BacklightCompensationMode_ON;
		p_item->BacklightCompensationFlag = 1;
		p_item->BacklightCompensation.LevelFlag = 1;
		p_item->BacklightCompensation.Level = capCfg->backlight;
	}
	else
	{
		p_item->BacklightCompensation.Mode = BacklightCompensationMode_OFF;
		p_item->BacklightCompensationFlag = 1;
		p_item->BacklightCompensation.LevelFlag = 1;
		p_item->BacklightCompensation.Level = 0;
	}
	if (1)
	{
		p_item->BrightnessFlag = 1;
		p_item->Brightness = capCfg->brightness;
		p_item->ColorSaturationFlag = 1;
		p_item->ColorSaturation = capCfg->saturation;
		p_item->ContrastFlag = 1;
		p_item->Contrast = capCfg->contrast;
		
	}
	p_item->SharpnessFlag = 1;
	p_item->Sharpness = capCfg->sharpness;
	if (1)
	{
		p_item->ExposureFlag = 1;
		p_item->Exposure.Mode = capCfg->shutterSetting.shutter_mode_day == ExposureMode_AUTO ? ExposureMode_AUTO : ExposureMode_MANUAL;
		
		p_item->Exposure.PriorityFlag = 0;
		p_item->Exposure.Priority = ExposurePriority_LowNoise;

		p_item->Exposure.Window.bottom = 0;
		p_item->Exposure.Window.top = 0;
		p_item->Exposure.Window.right = 0;
		p_item->Exposure.Window.left = 0;
		
		p_item->Exposure.MinExposureTimeFlag = 1;
		p_item->Exposure.MinExposureTime = 100;
		
		p_item->Exposure.MaxExposureTimeFlag = 1;
		p_item->Exposure.MaxExposureTime = 100000;
		
		p_item->Exposure.MinGainFlag = 0;
		p_item->Exposure.MinGain = 0;
		
		p_item->Exposure.MaxGainFlag = 0;
		p_item->Exposure.MaxGain = 100;
		
		p_item->Exposure.MinIrisFlag = 0;
		p_item->Exposure.MinIris = 0;
		
		p_item->Exposure.MaxIrisFlag = 0;
		p_item->Exposure.MaxIris = 0;
		
		p_item->Exposure.ExposureTimeFlag = 1;
		p_item->Exposure.ExposureTime = capCfg->shutterSetting.shutter_speed_day;

		p_item->Exposure.GainFlag = 0;
		p_item->Exposure.Gain = 0;
		
		p_item->Exposure.IrisFlag = 0;
		p_item->Exposure.Iris = 0;
	}
	if (1)
	{
		p_item->FocusFlag = 1;
		p_item->Focus.AutoFocusMode = AutoFocusMode_MANUAL;

		p_item->Focus.DefaultSpeedFlag = 1;
		p_item->Focus.DefaultSpeed = 1;

		p_item->Focus.NearLimitFlag = 1;
		p_item->Focus.NearLimit = 1;
		
		p_item->Focus.FarLimitFlag = 1;
		p_item->Focus.FarLimit = 10;
	}
	if (1)
	{
		p_item->IrCutFilterFlag = 1;
		if(capCfg->ircut_mode == IRCUT_Mode_Passive)
			p_item->IrCutFilter = IrCutFilterMode_AUTO;
		else if(capCfg->ircut_mode == IRCUT_Mode_Manual)
		{
			if( capCfg->ircut_keepcolor == 0 )
				p_item->IrCutFilter = IrCutFilterMode_OFF;//off
			else 
				p_item->IrCutFilter = IrCutFilterMode_ON;//on
		}
		else
			p_item->IrCutFilter = IrCutFilterMode_AUTO;
	}
	
	p_item->WideDynamicRangeFlag = 1;
	p_item->WideDynamicRange.Mode = capCfg->wdr_mode == 0 ? WideDynamicMode_OFF : WideDynamicMode_ON;
	
	p_item->WideDynamicRange.LevelFlag = 1;
#if !PLATFORM_DM365
		p_item->WideDynamicRange.Level = capCfg->wdr_value;
#else
		p_item->WideDynamicRange.Level = 0;
#endif
	if(p_item->WideDynamicRange.Level > 255)
		p_item->WideDynamicRange.Level = 255;
	else if(p_item->WideDynamicRange.Level < 0)
		p_item->WideDynamicRange.Level = 0;
	
	p_item->WhiteBalanceFlag = 1;
	if((capCfg->whitebalance>>24)&0xff)
		p_item->WhiteBalance.Mode = WhiteBalanceMode_MANUAL;
	else
		p_item->WhiteBalance.Mode = WhiteBalanceMode_AUTO;
	
	p_item->WhiteBalance.CbGainFlag = 1;
	p_item->WhiteBalance.CbGain = (capCfg->whitebalance) & 0xff;
	
	p_item->WhiteBalance.CrGainFlag = 1;
	p_item->WhiteBalance.CrGain = (capCfg->whitebalance>>16) & 0xff;;

	if (1)
	{
		p_item->ExtensionFlag = 1;
		p_item->Extension.Extension202Flag = 1;
		p_item->Extension.Extension.Extension203Flag = 1;
		
		p_item->Extension.Extension.Extension.DefoggingFlag = 1;
		if (capCfg->dfrog_flag != 0)
			strcpy(p_item->Extension.Extension.Extension.Defogging.Mode, "ON");
		else
			strcpy(p_item->Extension.Extension.Extension.Defogging.Mode, "OFF");
		p_item->Extension.Extension.Extension.Defogging.Level = (float)capCfg->dfrog_value/255; 
		
		p_item->Extension.Extension.Extension.NoiseReductionFlag = 1;
		p_item->Extension.Extension.Extension.NoiseReduction.Level = (float)capCfg->tnf/255;
	}
	return;
}

HT_API void onvif_init_ImagingOptions(VideoSourceList * p_vsrc, onvif_ImagingOptions * p_item)
{
	// init image config options
	// note : Optional field flag is set to 0, this option will not appear
	if (1)
	{
		p_item->BacklightCompensationFlag = 1;
		p_item->BacklightCompensation.Mode_OFF = 0;
		p_item->BacklightCompensation.Mode_ON = 1;

		p_item->BacklightCompensation.LevelFlag = 1;
		p_item->BacklightCompensation.Level.Min = 0;
		p_item->BacklightCompensation.Level.Max = 255;

		p_item->BrightnessFlag = 1;
		p_item->Brightness.Min = 1;
		p_item->Brightness.Max = 255;


		p_item->ContrastFlag = 1;
		p_item->Contrast.Min = 1;
		p_item->Contrast.Max = 255;
		
		p_item->ColorSaturationFlag = 1;
		p_item->ColorSaturation.Min = 1;
		p_item->ColorSaturation.Max = 255;

		p_item->SharpnessFlag = 1;
		p_item->Sharpness.Min = 1;
		p_item->Sharpness.Max = 255;
		
		p_item->IrCutFilterMode_ON = 1;
		p_item->IrCutFilterMode_OFF = 1;
		p_item->IrCutFilterMode_AUTO = 1;
	}

	if (1)
	{
		p_item->ExposureFlag = 1;
		p_item->Exposure.Mode_AUTO = 1;
		p_item->Exposure.Mode_MANUAL = 1;
		p_item->Exposure.Priority_LowNoise = 0;
		p_item->Exposure.Priority_FrameRate = 0;

		p_item->Exposure.MinExposureTimeFlag = 0;
		p_item->Exposure.MinExposureTime.Min = 10;
		p_item->Exposure.MinExposureTime.Max = 10;
		
		p_item->Exposure.MaxExposureTimeFlag = 0;
		p_item->Exposure.MaxExposureTime.Min = 10;
		p_item->Exposure.MaxExposureTime.Max = 320000;
		
		p_item->Exposure.MinGainFlag = 0;
		p_item->Exposure.MinGain.Min = 0;
		p_item->Exposure.MinGain.Max = 0;
		
		p_item->Exposure.MaxGainFlag = 0;
		p_item->Exposure.MaxGain.Min = 0;
		p_item->Exposure.MaxGain.Max = 100;
		
		p_item->Exposure.MinIrisFlag = 0;
		p_item->Exposure.MinIris.Min = 0;
		p_item->Exposure.MinIris.Max = 10;
		
		p_item->Exposure.MaxIrisFlag = 0;
		p_item->Exposure.MaxIris.Min = 0;
		p_item->Exposure.MaxIris.Max = 10;

		p_item->Exposure.GainFlag = 0;
		p_item->Exposure.Gain.Min = 0;
		p_item->Exposure.Gain.Max = 100;

		p_item->Exposure.IrisFlag = 0;
		p_item->Exposure.Iris.Min = 0;
		p_item->Exposure.Iris.Max = 100;
		
		p_item->Exposure.ExposureTimeFlag = 1;
		p_item->Exposure.ExposureTime.Min = 100;
		p_item->Exposure.ExposureTime.Max = 100000;
	}
	
	if (1)
	{
		p_item->FocusFlag = 1;
		p_item->Focus.AutoFocusModes_AUTO = 0;
		p_item->Focus.AutoFocusModes_MANUAL = 1;
	
		p_item->Focus.DefaultSpeedFlag = 1;
		p_item->Focus.DefaultSpeed.Min = 1;
		p_item->Focus.DefaultSpeed.Max = 10;
		
		p_item->Focus.NearLimitFlag = 0;
		p_item->Focus.NearLimit.Min = 1;
		p_item->Focus.NearLimit.Max = 10;

		p_item->Focus.FarLimitFlag = 0;
		p_item->Focus.FarLimit.Min = 1;
		p_item->Focus.FarLimit.Max = 10;

		p_item->WideDynamicRangeFlag = 1;
		p_item->WideDynamicRange.Mode_OFF = 1;
		p_item->WideDynamicRange.Mode_ON = 1;

		p_item->WideDynamicRange.LevelFlag = 1;
		p_item->WideDynamicRange.Level.Min = 0;
		p_item->WideDynamicRange.Level.Max = 255;

		p_item->WhiteBalanceFlag = 1;
		p_item->WhiteBalance.Mode_AUTO = 1;
		p_item->WhiteBalance.Mode_MANUAL = 1;

		p_item->WhiteBalance.YrGainFlag = 1;
		p_item->WhiteBalance.YrGain.Min = 0;
		p_item->WhiteBalance.YrGain.Max = 255;

		p_item->WhiteBalance.YbGainFlag = 1;
		p_item->WhiteBalance.YbGain.Min = 0;
		p_item->WhiteBalance.YbGain.Max = 255;
	}
	if (1)
	{
		p_item->ExtensionFlag = 1;
		p_item->Extension.Extension202Flag = 1;
		p_item->Extension.Extension.Extension203Flag = 1;

		p_item->Extension.Extension.Extension.DefoggingOptionsFlag = 1;
		p_item->Extension.Extension.Extension.DefoggingOptions.Level = TRUE;
		p_item->Extension.Extension.Extension.DefoggingOptions.__sizeMode = 2;
		strcpy(&p_item->Extension.Extension.Extension.DefoggingOptions.Mode[0][0], "OFF");
		strcpy(&p_item->Extension.Extension.Extension.DefoggingOptions.Mode[1][0], "ON");

		p_item->Extension.Extension.Extension.NoiseReductionOptionsFlag = 1;
		p_item->Extension.Extension.Extension.NoiseReductionOptions.Level = TRUE;
	}
	return;
}

#endif // end of IMAGE_SUPPORT

HT_API void onvif_init_capabilities()
{
#ifdef DEVICEIO_SUPPORT
	int vsrc = 0, vout = 0;
	int relay_output = 0, serial_port = 0, digit_input = 0;
	VideoSourceList    * p_vsrc;
	VideoOutputList    * p_vout;
	RelayOutputList    * p_relay_output;
	SerialPortList     * p_serial_port;
	DigitalInputList   * p_digital_input;	
#ifdef AUDIO_SUPPORT
	int asrc = 0, aout = 0;
	AudioSourceList    * p_asrc;
	AudioOutputList    * p_aout;
#endif    
#endif

	// network capabilities

#ifdef IPFILTER_SUPPORT	
	g_onvif_cfg.Capabilities.device.IPFilter = 0;
#endif
	g_onvif_cfg.Capabilities.device.ZeroConfiguration = 0;
	g_onvif_cfg.Capabilities.device.IPVersion6 = 0;
	g_onvif_cfg.Capabilities.device.DynDNS = 0;
	g_onvif_cfg.Capabilities.device.HostnameFromDHCP = 0;
	g_onvif_cfg.Capabilities.device.DHCPv6 = 0;

	// system capabilities
	g_onvif_cfg.Capabilities.device.DiscoveryResolve = 0;
	g_onvif_cfg.Capabilities.device.DiscoveryBye = 0;
	g_onvif_cfg.Capabilities.device.RemoteDiscovery = 0;
	g_onvif_cfg.Capabilities.device.SystemBackup = 0;
	g_onvif_cfg.Capabilities.device.SystemLogging = 1;
	g_onvif_cfg.Capabilities.device.FirmwareUpgrade = 0;
	g_onvif_cfg.Capabilities.device.HttpFirmwareUpgrade = 1;
	g_onvif_cfg.Capabilities.device.HttpSystemBackup = 0;
	g_onvif_cfg.Capabilities.device.HttpSystemLogging = 1;
	g_onvif_cfg.Capabilities.device.HttpSupportInformation = 1;
	g_onvif_cfg.Capabilities.device.DiscoveryNotSupported = 0;
	g_onvif_cfg.Capabilities.device.NetworkConfigNotSupported = 0;
	g_onvif_cfg.Capabilities.device.UserConfigNotSupported = 0;

	g_onvif_cfg.Capabilities.device.sizeSupportedVersions = 10;
	g_onvif_cfg.Capabilities.device.SupportedVersions[0].Major = 23;
	g_onvif_cfg.Capabilities.device.SupportedVersions[0].Minor = 12;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[1].Major = 23;
	g_onvif_cfg.Capabilities.device.SupportedVersions[1].Minor = 6;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[2].Major = 22;
	g_onvif_cfg.Capabilities.device.SupportedVersions[2].Minor = 12;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[3].Major = 22;
	g_onvif_cfg.Capabilities.device.SupportedVersions[3].Minor = 6;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[4].Major = 21;
	g_onvif_cfg.Capabilities.device.SupportedVersions[4].Minor = 12;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[5].Major = 21;
	g_onvif_cfg.Capabilities.device.SupportedVersions[5].Minor = 6;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[6].Major = 20;
	g_onvif_cfg.Capabilities.device.SupportedVersions[6].Minor = 12;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[7].Major = 20;
	g_onvif_cfg.Capabilities.device.SupportedVersions[7].Minor = 6;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[8].Major = 19;
	g_onvif_cfg.Capabilities.device.SupportedVersions[8].Minor = 12;
	
	g_onvif_cfg.Capabilities.device.SupportedVersions[9].Major = 2;
	g_onvif_cfg.Capabilities.device.SupportedVersions[9].Minor = 0;

	g_onvif_cfg.Capabilities.device.Version.Major = 23;
	g_onvif_cfg.Capabilities.device.Version.Minor = 12;

#ifdef STORAGE_SUPPORT
	g_onvif_cfg.Capabilities.device.StorageConfiguration = 1;
	g_onvif_cfg.Capabilities.device.MaxStorageConfigurations = 10;
#endif

#ifdef GEOLOCATION_SUPPORT
	g_onvif_cfg.Capabilities.device.GeoLocationEntries = 10;
#endif

#ifdef DOT11_SUPPORT
	g_onvif_cfg.Capabilities.device.Dot11Configuration = 0;
	g_onvif_cfg.Capabilities.device.Dot1XConfigurations = 1;
	// dot11 capabilities
	g_onvif_cfg.Capabilities.dot11.TKIP = 1;
	g_onvif_cfg.Capabilities.dot11.ScanAvailableNetworks = 1;
	g_onvif_cfg.Capabilities.dot11.MultipleConfiguration = 0;
	g_onvif_cfg.Capabilities.dot11.AdHocStationMode = 1;
	g_onvif_cfg.Capabilities.dot11.WEP = 1;
#endif

	// scurity capabilities
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		g_onvif_cfg.Capabilities.device.TLS10 = 0;
		g_onvif_cfg.Capabilities.device.TLS11 = 0;
		g_onvif_cfg.Capabilities.device.TLS12 = 0;
	}
#endif

	g_onvif_cfg.Capabilities.device.OnboardKeyGeneration = 0;
	g_onvif_cfg.Capabilities.device.AccessPolicyConfig = 0;
	g_onvif_cfg.Capabilities.device.DefaultAccessPolicy = 0;
	g_onvif_cfg.Capabilities.device.Dot1X = 0;
	g_onvif_cfg.Capabilities.device.RemoteUserHandling = 0;
	g_onvif_cfg.Capabilities.device.X509Token = 0;
	g_onvif_cfg.Capabilities.device.SAMLToken = 0;
	g_onvif_cfg.Capabilities.device.KerberosToken = 0;
	g_onvif_cfg.Capabilities.device.UsernameToken = 0;
	g_onvif_cfg.Capabilities.device.HttpDigest = 0;
	g_onvif_cfg.Capabilities.device.RELToken = 0;
	g_onvif_cfg.Capabilities.device.JsonWebToken = 0;

	g_onvif_cfg.Capabilities.device.NTP = MAX_NTP_SERVER;
	g_onvif_cfg.Capabilities.device.SupportedEAPMethods = 0;
	g_onvif_cfg.Capabilities.device.MaxUsers = MAX_USERS;
	g_onvif_cfg.Capabilities.device.MaxUserNameLength = 32;
	g_onvif_cfg.Capabilities.device.MaxPasswordLength = 32;

	if (g_onvif_cfg.md5_hashing && g_onvif_cfg.sha256_hashing)
	{
		strcpy(g_onvif_cfg.Capabilities.device.HashingAlgorithms, "MD5,SHA-256");
	}
	else if (g_onvif_cfg.md5_hashing)
	{
		strcpy(g_onvif_cfg.Capabilities.device.HashingAlgorithms, "MD5");
	}
	else if (g_onvif_cfg.sha256_hashing)
	{
		strcpy(g_onvif_cfg.Capabilities.device.HashingAlgorithms, "SHA-256");
	}

#ifdef DEVICEIO_SUPPORT
	g_onvif_cfg.Capabilities.device.InputConnectors = 1;
	g_onvif_cfg.Capabilities.device.RelayOutputs = ALARM_SUPPORT_IO_OUT;
#endif

#ifdef MEDIA_SUPPORT
	// media capabilities
	g_onvif_cfg.Capabilities.media.SnapshotUri = 1;
	g_onvif_cfg.Capabilities.media.Rotation = 0;
	g_onvif_cfg.Capabilities.media.VideoSourceMode = 0;
	g_onvif_cfg.Capabilities.media.OSD = 1;
	g_onvif_cfg.Capabilities.media.TemporaryOSDText = 0;
	g_onvif_cfg.Capabilities.media.EXICompression = 0;
	
	g_onvif_cfg.Capabilities.media.RTPMulticast = 1;
	g_onvif_cfg.Capabilities.media.RTP_TCP = 1;
	g_onvif_cfg.Capabilities.media.RTP_RTSP_TCP = 1;
	g_onvif_cfg.Capabilities.media.NonAggregateControl = 0;
	g_onvif_cfg.Capabilities.media.NoRTSPStreaming = 0;/**/
	
	g_onvif_cfg.Capabilities.media.support = 1;
	g_onvif_cfg.Capabilities.media.Version.Major = 22;
	g_onvif_cfg.Capabilities.media.Version.Minor = 12;

	g_onvif_cfg.Capabilities.media.MaximumNumberOfProfiles = 10;
#endif // MEDIA_SUPPORT

#ifdef MEDIA2_SUPPORT
	// media2 capabilities
	g_onvif_cfg.Capabilities.media2.SnapshotUri = 1;
	g_onvif_cfg.Capabilities.media2.Rotation = 0;
	g_onvif_cfg.Capabilities.media2.VideoSourceMode = 0;
	g_onvif_cfg.Capabilities.media2.OSD = 1;
	g_onvif_cfg.Capabilities.media2.TemporaryOSDText = 0;
	g_onvif_cfg.Capabilities.media2.Mask = 1;
	g_onvif_cfg.Capabilities.media2.SourceMask = 1;

	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTP_RTSP_TCP = 1;
	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTSPStreaming = 1;
	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.RTPMulticast = 1;
	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.AutoStartMulticast = 0;
	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.SecureRTSPStreaming = 0;
	g_onvif_cfg.Capabilities.media2.StreamingCapabilities.NonAggregateControl = 0;
	
	g_onvif_cfg.Capabilities.media2.ProfileCapabilities.MaximumNumberOfProfilesFlag = 1;
	g_onvif_cfg.Capabilities.media2.ProfileCapabilities.MaximumNumberOfProfiles = 10;
	g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupportedFlag = 1;

	strcpy(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, "VideoSource VideoEncoder Metadata");

#ifdef PTZ_SUPPORT
	strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " PTZ");
#endif
#ifdef AUDIO_SUPPORT
	strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " AudioSource AudioEncoder");
#ifdef DEVICEIO_SUPPORT
	strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " AudioOutput AudioDecoder");
#endif
#endif

#ifdef VIDEO_ANALYTICS
	strcat(g_onvif_cfg.Capabilities.media2.ProfileCapabilities.ConfigurationsSupported, " Analytics");
#endif

	g_onvif_cfg.Capabilities.media2.support = 1;
	g_onvif_cfg.Capabilities.media2.Version.Major = 23;
	g_onvif_cfg.Capabilities.media2.Version.Minor = 6;
#endif // end of MEDIA2_SUPPORT

	// event capabilities
	g_onvif_cfg.Capabilities.events.WSSubscriptionPolicySupport = 1;
	g_onvif_cfg.Capabilities.events.WSPullPointSupport = 1;
	g_onvif_cfg.Capabilities.events.WSPausableSubscriptionManagerInterfaceSupport = 1;
	g_onvif_cfg.Capabilities.events.PersistentNotificationStorage = 0;
	g_onvif_cfg.Capabilities.events.support = 1;
	g_onvif_cfg.Capabilities.events.Version.Major = 22;
	g_onvif_cfg.Capabilities.events.Version.Minor = 6;

	g_onvif_cfg.Capabilities.events.MaxNotificationProducers = 10;
	g_onvif_cfg.Capabilities.events.MaxPullPoints = 10;

#ifdef IMAGE_SUPPORT
	// image capabilities
	g_onvif_cfg.Capabilities.image.ImageStabilization = 0;
	g_onvif_cfg.Capabilities.image.Presets = 1;
	g_onvif_cfg.Capabilities.image.AdaptablePreset = 0;
	g_onvif_cfg.Capabilities.image.support = 1;
	g_onvif_cfg.Capabilities.image.Version.Major = 22;
	g_onvif_cfg.Capabilities.image.Version.Minor = 6;
#endif // IMAGE_SUPPORT

#ifdef PTZ_SUPPORT
	// ptz capabilities
	g_onvif_cfg.Capabilities.ptz.EFlip = 1;
	g_onvif_cfg.Capabilities.ptz.Reverse = 1;
	g_onvif_cfg.Capabilities.ptz.GetCompatibleConfigurations = 1;
	g_onvif_cfg.Capabilities.ptz.MoveStatus = 1;
	g_onvif_cfg.Capabilities.ptz.StatusPosition = 1;
	g_onvif_cfg.Capabilities.ptz.support = 1;
	g_onvif_cfg.Capabilities.ptz.Version.Major = 23;
	g_onvif_cfg.Capabilities.ptz.Version.Minor = 6;
#endif // end of PTZ_SUPPORT

#ifdef VIDEO_ANALYTICS
	// analytics capabilities
	g_onvif_cfg.Capabilities.analytics.RuleSupport = 1;
	g_onvif_cfg.Capabilities.analytics.AnalyticsModuleSupport = 1;
	g_onvif_cfg.Capabilities.analytics.CellBasedSceneDescriptionSupported = 1;
	g_onvif_cfg.Capabilities.analytics.RuleOptionsSupported = 1;
	g_onvif_cfg.Capabilities.analytics.AnalyticsModuleOptionsSupported = 1;
	g_onvif_cfg.Capabilities.analytics.SupportedMetadata = 1;
	g_onvif_cfg.Capabilities.analytics.support = 1;
	g_onvif_cfg.Capabilities.analytics.Version.Major = 23;
	g_onvif_cfg.Capabilities.analytics.Version.Minor = 12;
#endif // end of VIDEO_ANALYTICS

#ifdef PROFILE_G_SUPPORT
	// record capabilities
	g_onvif_cfg.Capabilities.recording.ReceiverSource = 0;
	g_onvif_cfg.Capabilities.recording.MediaProfileSource = 1;
	g_onvif_cfg.Capabilities.recording.DynamicRecordings = 1;
	g_onvif_cfg.Capabilities.recording.DynamicTracks = 1;
	g_onvif_cfg.Capabilities.recording.Options = 1;
	g_onvif_cfg.Capabilities.recording.MetadataRecording = 1;
	g_onvif_cfg.Capabilities.recording.JPEG = 1;
#ifdef MPEG4_SUPPORT    
	g_onvif_cfg.Capabilities.recording.MPEG4 = 1;
#endif    
	g_onvif_cfg.Capabilities.recording.H264 = 1;
	g_onvif_cfg.Capabilities.recording.H265 = 1;
#ifdef AUDIO_SUPPORT    
	g_onvif_cfg.Capabilities.recording.G711 = 1;
	g_onvif_cfg.Capabilities.recording.G726 = 1;
	g_onvif_cfg.Capabilities.recording.AAC = 1;
#endif
	/* Keep advanced Profile-G services disabled until full backend is completed. */
	g_onvif_cfg.Capabilities.recording.support = 0;
	g_onvif_cfg.Capabilities.recording.Version.Major = 23;
	g_onvif_cfg.Capabilities.recording.Version.Minor = 6;

	g_onvif_cfg.Capabilities.recording.MaxStringLength = 256;
	g_onvif_cfg.Capabilities.recording.MaxRate = 200;
	g_onvif_cfg.Capabilities.recording.MaxTotalRate = 2000;
	g_onvif_cfg.Capabilities.recording.MaxRecordings = 5;
	g_onvif_cfg.Capabilities.recording.MaxRecordingJobs = 5;

	// search capabilities
	g_onvif_cfg.Capabilities.search.MetadataSearch = 1;
	g_onvif_cfg.Capabilities.search.GeneralStartEvents = 1;
	g_onvif_cfg.Capabilities.search.support = 0;
	g_onvif_cfg.Capabilities.search.Version.Major = 22;
	g_onvif_cfg.Capabilities.search.Version.Minor = 6;

	// replay capabilities
	g_onvif_cfg.Capabilities.replay.ReversePlayback = 0;
	g_onvif_cfg.Capabilities.replay.RTP_RTSP_TCP = 1;
	g_onvif_cfg.Capabilities.replay.support = 0;
	g_onvif_cfg.Capabilities.replay.Version.Major = 21;
	g_onvif_cfg.Capabilities.replay.Version.Minor = 12;

	g_onvif_cfg.Capabilities.replay.SessionTimeoutRange.Min = 10;
	g_onvif_cfg.Capabilities.replay.SessionTimeoutRange.Max = 100;
#endif // end of PROFILE_G_SUPPORT

#ifdef PROFILE_C_SUPPORT
	// accesscontrol capabilities
	g_onvif_cfg.Capabilities.accesscontrol.support = 1;
	g_onvif_cfg.Capabilities.accesscontrol.Version.Major = 21;
	g_onvif_cfg.Capabilities.accesscontrol.Version.Minor = 6;
	g_onvif_cfg.Capabilities.accesscontrol.MaxLimit = ACCESS_CTRL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.accesscontrol.MaxAccessPoints = ACCESS_CTRL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.accesscontrol.MaxAreas = ACCESS_CTRL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.accesscontrol.ClientSuppliedTokenSupported = 1;
	g_onvif_cfg.Capabilities.accesscontrol.AccessPointManagementSupported = 1;
	g_onvif_cfg.Capabilities.accesscontrol.AreaManagementSupported = 1;

	// doorcontrol capabilities
	g_onvif_cfg.Capabilities.doorcontrol.support = 0;
	g_onvif_cfg.Capabilities.doorcontrol.Version.Major = 21;
	g_onvif_cfg.Capabilities.doorcontrol.Version.Minor = 6;
	g_onvif_cfg.Capabilities.doorcontrol.MaxLimit = DOOR_CTRL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.doorcontrol.MaxDoors = DOOR_CTRL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.doorcontrol.ClientSuppliedTokenSupported = 1;
	g_onvif_cfg.Capabilities.doorcontrol.DoorManagementSupported = 1;
#endif // end of PROFILE_C_SUPPORT

#ifdef DEVICEIO_SUPPORT
	p_vsrc = g_onvif_cfg.v_src;
	while (p_vsrc)
	{
		vsrc++;

		p_vsrc = p_vsrc->next;
	}

	p_vout = g_onvif_cfg.v_output;
	while (p_vout)
	{
		vout++;

		p_vout = p_vout->next;
	}

#ifdef AUDIO_SUPPORT
	p_asrc = g_onvif_cfg.a_src;
	while (p_asrc)
	{
		asrc++;

		p_asrc = p_asrc->next;
	}

	p_aout = g_onvif_cfg.a_output;
	while (p_aout)
	{
		aout++;

		p_aout = p_aout->next;
	}
#endif

	p_relay_output = g_onvif_cfg.relay_output;
	while (p_relay_output)
	{
		relay_output++;

		p_relay_output = p_relay_output->next;
	}

	p_serial_port = g_onvif_cfg.serial_port;
	while (p_serial_port)
	{
		serial_port++;

		p_serial_port = p_serial_port->next;
	}

	p_digital_input = g_onvif_cfg.digit_input;
	while (p_digital_input)
	{
		digit_input++;

		p_digital_input = p_digital_input->next;
	}

	// deviceIO capabilities
	g_onvif_cfg.Capabilities.deviceIO.support = 0;
	g_onvif_cfg.Capabilities.deviceIO.Version.Major = 22;
	g_onvif_cfg.Capabilities.deviceIO.Version.Minor = 6;
	g_onvif_cfg.Capabilities.deviceIO.VideoSourcesFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.VideoSources = vsrc;
	g_onvif_cfg.Capabilities.deviceIO.VideoOutputsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.VideoOutputs = vout;
#ifdef AUDIO_SUPPORT    
	g_onvif_cfg.Capabilities.deviceIO.AudioSourcesFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.AudioSources = asrc;
	g_onvif_cfg.Capabilities.deviceIO.AudioOutputsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.AudioOutputs = aout;
#endif    
	g_onvif_cfg.Capabilities.deviceIO.RelayOutputsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.RelayOutputs = relay_output;
	g_onvif_cfg.Capabilities.deviceIO.SerialPortsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.SerialPorts = serial_port;
	g_onvif_cfg.Capabilities.deviceIO.DigitalInputsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.DigitalInputs = digit_input;
	g_onvif_cfg.Capabilities.deviceIO.DigitalInputOptionsFlag = 1;
	g_onvif_cfg.Capabilities.deviceIO.DigitalInputOptions = TRUE;
#endif // end of DEVICEIO_SUPPORT

#ifdef THERMAL_SUPPORT
	// thermal capabilities
	g_onvif_cfg.Capabilities.thermal.support = 1;
	g_onvif_cfg.Capabilities.thermal.Version.Major = 22;
	g_onvif_cfg.Capabilities.thermal.Version.Minor = 6;
	g_onvif_cfg.Capabilities.thermal.Radiometry = 1;
#endif // end of THERMAL_SUPPORT

#ifdef CREDENTIAL_SUPPORT
	// credential capabilities
	g_onvif_cfg.Capabilities.credential.support = 0;
	g_onvif_cfg.Capabilities.credential.Version.Major = 21;
	g_onvif_cfg.Capabilities.credential.Version.Minor = 6;
	g_onvif_cfg.Capabilities.credential.CredentialValiditySupported = 1;
	g_onvif_cfg.Capabilities.credential.CredentialAccessProfileValiditySupported = 1;
	g_onvif_cfg.Capabilities.credential.ValiditySupportsTimeValue = 1;
	g_onvif_cfg.Capabilities.credential.ResetAntipassbackSupported = 1;
	g_onvif_cfg.Capabilities.credential.ClientSuppliedTokenSupported = 1;
	g_onvif_cfg.Capabilities.credential.MaxLimit = CREDENTIAL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.credential.MaxCredentials = CREDENTIAL_MAX_LIMIT;
	g_onvif_cfg.Capabilities.credential.MaxAccessProfilesPerCredential = CREDENTIAL_MAX_LIMIT;

	g_onvif_cfg.Capabilities.credential.sizeSupportedIdentifierType = 3;
	strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[0], "pt:Card");
	strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[1], "pt:PIN");
	strcpy(g_onvif_cfg.Capabilities.credential.SupportedIdentifierType[2], "pt:Fingerprint");

	strcpy(g_onvif_cfg.Capabilities.credential.DefaultCredentialSuspensionDuration, "PT5M");

	g_onvif_cfg.Capabilities.credential.MaxWhitelistedItems = 0;
	g_onvif_cfg.Capabilities.credential.MaxBlacklistedItems = 0;

	g_onvif_cfg.Capabilities.credential.ExtensionFlag = 0;
	g_onvif_cfg.Capabilities.credential.Extension.sizeSupportedExemptionType = 0;
#endif // end of CREDENTIAL_SUPPORT

#ifdef ACCESS_RULES
	// access rules capabilities
	g_onvif_cfg.Capabilities.accessrules.support = 0;
	g_onvif_cfg.Capabilities.accessrules.Version.Major = 19;
	g_onvif_cfg.Capabilities.accessrules.Version.Minor = 6;
	g_onvif_cfg.Capabilities.accessrules.MaxLimit = ACCESSRULES_MAX_LIMIT;
	g_onvif_cfg.Capabilities.accessrules.MaxAccessProfiles = ACCESSRULES_MAX_LIMIT;
	g_onvif_cfg.Capabilities.accessrules.MaxAccessPoliciesPerAccessProfile = 1;
	g_onvif_cfg.Capabilities.accessrules.MultipleSchedulesPerAccessPointSupported = 1;
	g_onvif_cfg.Capabilities.accessrules.ClientSuppliedTokenSupported = 1;
#endif // end of ACCESS_RULES

#ifdef SCHEDULE_SUPPORT
	g_onvif_cfg.Capabilities.schedule.support = 0;
	g_onvif_cfg.Capabilities.schedule.Version.Major = 18;
	g_onvif_cfg.Capabilities.schedule.Version.Minor = 12;
	g_onvif_cfg.Capabilities.schedule.MaxLimit = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.MaxSchedules = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.MaxTimePeriodsPerDay = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.MaxSpecialDayGroups = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.MaxDaysInSpecialDayGroup = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.MaxSpecialDaysSchedules = SCHEDULE_MAX_LIMIT;
	g_onvif_cfg.Capabilities.schedule.ExtendedRecurrenceSupported = 1;
	g_onvif_cfg.Capabilities.schedule.SpecialDaysSupported = 1;
	g_onvif_cfg.Capabilities.schedule.StateReportingSupported = 0;
	g_onvif_cfg.Capabilities.schedule.ClientSuppliedTokenSupported = 0;
#endif // end of SCHEDULE_SUPPORT

#ifdef RECEIVER_SUPPORT
	g_onvif_cfg.Capabilities.receiver.support = 0;
	g_onvif_cfg.Capabilities.receiver.Version.Major = 21;
	g_onvif_cfg.Capabilities.receiver.Version.Minor = 12;
	g_onvif_cfg.Capabilities.receiver.RTP_USCOREMulticast = 1;
	g_onvif_cfg.Capabilities.receiver.RTP_USCORETCP = 1;
	g_onvif_cfg.Capabilities.receiver.RTP_USCORERTSP_USCORETCP = 1;

	g_onvif_cfg.Capabilities.receiver.SupportedReceivers = 10;
	g_onvif_cfg.Capabilities.receiver.MaximumRTSPURILength = 256;
#endif // end of RECEIVER_SUPPORT

#ifdef PROVISIONING_SUPPORT
	g_onvif_cfg.Capabilities.provisioning.support = 0;
	g_onvif_cfg.Capabilities.provisioning.Version.Major = 18;
	g_onvif_cfg.Capabilities.provisioning.Version.Minor = 12;
	g_onvif_cfg.Capabilities.provisioning.DefaultTimeout = 60;
	g_onvif_cfg.Capabilities.provisioning.sizeSource = 1;

	if (g_onvif_cfg.v_src)
	{
	    strcpy(g_onvif_cfg.Capabilities.provisioning.Source[0].VideoSourceToken, g_onvif_cfg.v_src->VideoSource.token);
	}

	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumPanMovesFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumPanMoves = 60;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumTiltMovesFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumTiltMoves = 60;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumZoomMovesFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumZoomMoves = 60;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumRollMovesFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumRollMoves = 60;
	g_onvif_cfg.Capabilities.provisioning.Source[0].AutoLevelFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].AutoLevel = TRUE;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumFocusMovesFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].MaximumFocusMoves = 60;
	g_onvif_cfg.Capabilities.provisioning.Source[0].AutoFocusFlag = 1;
	g_onvif_cfg.Capabilities.provisioning.Source[0].AutoFocus = TRUE;
#endif // end of PROVISIONING_SUPPORT

	return;
}

void onvif_init_DeviceInformation()
{
	log_print(HT_LOG_INFO, "onvif_init_DeviceInformation START\n");
	if (g_onvif_cfg.DeviceInformationFlag)
	{
		return;
	}
	
	if( strlen(g_OemInfo.manufacturer) == 0 )
		strcpy(g_onvif_cfg.DeviceInformation.Manufacturer, DEFAULT_ONVIF_NONE);
	else
		strcpy(g_onvif_cfg.DeviceInformation.Manufacturer, g_OemInfo.manufacturer);
	
	log_print(HT_LOG_INFO, "g_onvif_cfg.DeviceInformation.Manufacturer:%s\n", g_onvif_cfg.DeviceInformation.Manufacturer);
	if( strlen(g_AJoemInfo.szVersion) > 0  && strlen(g_AJoemInfo.szBuildtime) > 0 )
	{
		size_t fw_buf_len = sizeof(g_onvif_cfg.DeviceInformation.FirmwareVersion);
		size_t build_len = strlen(g_AJoemInfo.szBuildtime);
		size_t reserve_len = strlen("V") + strlen(" build ");
		size_t max_build_len = fw_buf_len - reserve_len - 1;
		size_t max_version_len = 0;

		if (build_len > max_build_len)
			build_len = max_build_len;
		max_version_len = fw_buf_len - reserve_len - build_len - 1;

		snprintf(g_onvif_cfg.DeviceInformation.FirmwareVersion,
		         fw_buf_len,
		         "V%.*s build %.*s",
		         (int)max_version_len,
		         g_AJoemInfo.szVersion,
		         (int)build_len,
		         g_AJoemInfo.szBuildtime);
	}
	else
	{
		FILE *fp = fopen("/etc/filesys.ver","rb");
		char filebuf[LARGE_INFO_LENGTH]="";
		char *ptr=NULL;
		if(fp)
		{
			int ret = 0;
			ret = fread(filebuf, 1, LARGE_INFO_LENGTH, fp);
			fclose(fp);

			if(ret > 0)
			{
				filebuf[ret]='\0';
				ptr = strstr(filebuf," ");
				if(ptr)
					snprintf(g_onvif_cfg.DeviceInformation.FirmwareVersion,
					         sizeof(g_onvif_cfg.DeviceInformation.FirmwareVersion),
					         "%.*s",
					         (int)(sizeof(g_onvif_cfg.DeviceInformation.FirmwareVersion) - 1),
					         ptr + 1);
				else
					snprintf(g_onvif_cfg.DeviceInformation.FirmwareVersion,
					         sizeof(g_onvif_cfg.DeviceInformation.FirmwareVersion),
					         "%.*s",
					         (int)(sizeof(g_onvif_cfg.DeviceInformation.FirmwareVersion) - 1),
					         filebuf);
				
				log_print(HT_LOG_INFO, "g_onvif_cfg.DeviceInformation.FirmwareVersion:%s\n", g_onvif_cfg.DeviceInformation.FirmwareVersion);
			}
		}
	}
	if(!strlen(g_onvif_cfg.DeviceInformation.FirmwareVersion))
	{
		strcpy(g_onvif_cfg.DeviceInformation.FirmwareVersion, "1.0.0");
	}
	
	DevInfo stDevInfo = {0};
	memcpy(&stDevInfo, getDevInfo(), sizeof(DevInfo));
	if(stDevInfo.stVersionInfo.fsVersion[0] != '\0')
	{
		char *ptr=strstr(stDevInfo.stVersionInfo.fsVersion, " ");
		if(ptr)
			*ptr = '\0';
		ptr = strstr(stDevInfo.stVersionInfo.fsVersion, "_V");
		if(ptr)
			*ptr = '\0';

		strcpy((char *)g_onvif_cfg.DeviceInformation.Model, stDevInfo.stVersionInfo.fsVersion);
	}
	else
	{
		strcpy((char *)g_onvif_cfg.DeviceInformation.Model, DEFAULT_ONVIF_NAME);
	}
	log_print(HT_LOG_INFO, "g_onvif_cfg.DeviceInformation.Model:%s\n", g_onvif_cfg.DeviceInformation.Model);
	
	if(strlen(g_SN) > 0)
		snprintf(g_onvif_cfg.DeviceInformation.SerialNumber,
		         sizeof(g_onvif_cfg.DeviceInformation.SerialNumber),
		         "%s",
		         g_SN);
	else
		snprintf(g_onvif_cfg.DeviceInformation.SerialNumber,
		         sizeof(g_onvif_cfg.DeviceInformation.SerialNumber),
		         "%s",
		         g_uuid);

	{
		static const char *hardware_id_prefix = "1419d68a-1dd2-11b2-a105-";
		size_t hardware_id_buf_len = sizeof(g_onvif_cfg.DeviceInformation.HardwareId);
		size_t max_uuid_len = hardware_id_buf_len - strlen(hardware_id_prefix) - 1;

		snprintf(g_onvif_cfg.DeviceInformation.HardwareId,
		         hardware_id_buf_len,
		         "%s%.*s",
		         hardware_id_prefix,
		         (int)max_uuid_len,
		         g_uuid);
	}
	
	log_print(HT_LOG_INFO, "g_onvif_cfg.DeviceInformation.HardwareId:%s\n", g_onvif_cfg.DeviceInformation.HardwareId);
	
	log_print(HT_LOG_INFO, "onvif_init_DeviceInformation OVER\n");
	return ;
}

void onvif_init_SystemDateTime()
{
	log_print(HT_LOG_INFO, "onvif_init_SystemDateTime START\n");
	g_onvif_cfg.SystemDateTime.TimeZoneFlag = 1;

	onvif_get_timezone(g_onvif_cfg.SystemDateTime.TimeZone.TZ, sizeof(g_onvif_cfg.SystemDateTime.TimeZone.TZ), &g_onvif_cfg.SystemDateTime.DaylightSavings);

	if (g_onvif_cfg.SystemDateTimeFlag)
	{
		return;
	}
	
	TimeConfig pTimeCfg;
	memcpy(&pTimeCfg, &((SystemConfig *)getSystemConfig())->timeCfg, sizeof(pTimeCfg));
	if (!strcmp(pTimeCfg.timeMode.modeName, TIME_MODE_NAME_NTP))
	{
		g_onvif_cfg.SystemDateTime.DateTimeType = SetDateTimeType_NTP;
	}
	else
	{
		g_onvif_cfg.SystemDateTime.DateTimeType = SetDateTimeType_Manual;
	}
	
	log_print(HT_LOG_INFO, "onvif_init_SystemDateTime OVER\n");
	return;
}

void onvif_refresh_User()
{
	log_print(HT_LOG_INFO, "onvif_refresh_User START\n");
	onvif_User users[MAX_USERS] = {0};

	UserConfig pUserCfg;
	memcpy(&pUserCfg, &((SystemConfig *)getSystemConfig())->userCfg, sizeof(pUserCfg));
	int i;
	for (i = 0; i < pUserCfg.count; i++)
	{
		users[i].fixed = 0;
		users[i].PasswordFlag = 1;

		if (!strcmp(pUserCfg.accounts[i].group.groupName, "Administrator") && !strcmp(pUserCfg.accounts[i].userName, "admin"))
		{
			users[i].fixed = 1;
		}
		if (!strcmp(pUserCfg.accounts[i].group.groupName, "Administrator"))
			users[i].UserLevel = UserLevel_Administrator;
		else if (!strcmp(pUserCfg.accounts[i].group.groupName, "Operator"))
			users[i].UserLevel = UserLevel_Operator;
		else if (!strcmp(pUserCfg.accounts[i].group.groupName, "Viewer"))
			users[i].UserLevel = UserLevel_User;
		else
			users[i].UserLevel = UserLevel_Anonymous;
		
		strcpy(users[i].Username, pUserCfg.accounts[i].userName);
		strcpy(users[i].Password, pUserCfg.accounts[i].password);
	}
	if (memcmp(users, g_onvif_cfg.users, sizeof(onvif_User) * MAX_USERS) != 0)
	{
		memset(g_onvif_cfg.users, 0, sizeof(onvif_User) * MAX_USERS);
		memcpy(g_onvif_cfg.users, users, sizeof(onvif_User) * MAX_USERS);
	}
	log_print(HT_LOG_INFO, "onvif_refresh_User OVER\n");
	return;
}

void onvif_init_User()
{
	log_print(HT_LOG_INFO, "onvif_init_User START\n");
	if (g_onvif_cfg.UsersFlag)
	{
		return;
	}
	UserConfig pUserCfg;
	memcpy(&pUserCfg, &((SystemConfig *)getSystemConfig())->userCfg, sizeof(pUserCfg));
	int i;
	for (i = 0; i < pUserCfg.count; i++)
	{
		//log_print(HT_LOG_INFO, "pUserCfg.accounts[%d].userName:%s\n", i, pUserCfg.accounts[i].userName);
		//log_print(HT_LOG_INFO, "pUserCfg.accounts[%d].group.groupName:%s\n", i, pUserCfg.accounts[i].group.groupName);
		//log_print(HT_LOG_INFO, "pUserCfg.accounts[%d].status:%s\n", i, pUserCfg.accounts[i].status);
		onvif_User user;
		user.fixed = 0;
		user.PasswordFlag = 1;

		if (!strcmp(pUserCfg.accounts[i].group.groupName, "Administrator") && !strcmp(pUserCfg.accounts[i].userName, "admin"))
		{
			user.fixed = 1;
		}
		if (!strcmp(pUserCfg.accounts[i].group.groupName, "Administrator"))
			user.UserLevel = UserLevel_Administrator;
		else if (!strcmp(pUserCfg.accounts[i].group.groupName, "Operator"))
			user.UserLevel = UserLevel_Operator;
		else if (!strcmp(pUserCfg.accounts[i].group.groupName, "Viewer"))
			user.UserLevel = UserLevel_User;
		else
			user.UserLevel = UserLevel_Anonymous;
		
		strcpy(user.Username, pUserCfg.accounts[i].userName);
		strcpy(user.Password, pUserCfg.accounts[i].password);
		onvif_add_user(&user);
	}
	log_print(HT_LOG_INFO, "onvif_init_User OVER\n");
	return;
}

void onvif_init_Scope()
{
	log_print(HT_LOG_INFO, "onvif_init_Scope START\n");
	if (g_onvif_cfg.ScopesFlag)
	{
		return;
	}
	onvif_add_scope("onvif://www.onvif.org/type/ptz", FALSE);
	onvif_add_scope("onvif://www.onvif.org/type/video_encoder", FALSE);
	onvif_add_scope("onvif://www.onvif.org/type/audio_encoder", FALSE);
	onvif_add_scope("onvif://www.onvif.org/name/ONVIF_ICAMERA", FALSE);
	onvif_add_scope("onvif://www.onvif.org/Profile/Streaming", FALSE);//Profile S
	
	char hardware[64] = {0};
	char szDefaultHardware[32] = {0};
	GetDeviceTypeStr(szDefaultHardware);
	sprintf(hardware, "onvif://www.onvif.org/hardware/%s", szDefaultHardware);
	
	onvif_add_scope(hardware, FALSE);
	onvif_add_scope("onvif://www.onvif.org/manufacturer/NONE", FALSE);
	if (g_isTTVersion)
	{
		onvif_add_scope("onvif://www.onvif.org/location/Chinese", FALSE);
		onvif_add_scope("onvif://www.onvif.org/location/Taipei", FALSE);
	}
	else
	{
		onvif_add_scope("onvif://www.onvif.org/location/China", FALSE);
		onvif_add_scope("onvif://www.onvif.org/location/Shenzhen", FALSE);
	}

	if (g_onvif_expand)
	{
		onvif_add_scope("onvif://www.onvif.org/register_status/offline", FALSE);
		char _SerialNumber[1024] = {0};
		char mac[MACH_ADDR_LENGTH];
		get_my_macaddr(mac);
		if(strlen(g_SN) > 0)
			sprintf(_SerialNumber,"onvif://www.onvif.org/serial/%s", g_SN);
		else
			sprintf(_SerialNumber,"onvif://www.onvif.org/serial/%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		onvif_add_scope("onvif://www.onvif.org/manufacturer/NONE", FALSE);
		onvif_add_scope("onvif://www.onvif.org/type/IPC", FALSE);
	}
	
	log_print(HT_LOG_INFO, "onvif_init_Scope OVER\n");
	return;
}

BOOL onvif_init_ZeroConfiguration()
{
	if (g_onvif_cfg.network.interfaces)
	{
		strcpy(g_onvif_cfg.network.ZeroConfiguration.InterfaceToken, g_onvif_cfg.network.interfaces->NetworkInterface.token);
	}
	g_onvif_cfg.network.ZeroConfiguration.Enabled = 1;/**/
	char Zero_Addresses[LARGE_INFO_LENGTH];
	if (1)/**/
	{
		NET_IPV4 ip;
		ip.int32 = net_get_ifaddr(ETH_NAME_LOCAL);
		sprintf(Zero_Addresses, "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]); 
	}
	if (g_onvif_cfg.network.ZeroConfiguration.Enabled)
	{
		if (g_onvif_cfg.network.interfaces)
		{
			g_onvif_cfg.network.ZeroConfiguration.sizeAddresses = 1;
			strcpy(g_onvif_cfg.network.ZeroConfiguration.Addresses[0], Zero_Addresses);
		}
	}
	else
	{
		g_onvif_cfg.network.ZeroConfiguration.sizeAddresses = 0;
	}
	return TRUE;
}

void onvif_init_NetworkInterface()
{
	log_print(HT_LOG_INFO, "onvif_init_NetworkInterface START\n");
	int i;
	int socket_fd;
	struct ifreq *ifr;
	struct ifconf conf;
	struct ifreq ifs[8];
	int num;

	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	conf.ifc_len = sizeof(ifs);
	conf.ifc_req = ifs;

	ioctl(socket_fd, SIOCGIFCONF, &conf);

	num = conf.ifc_len / sizeof(struct ifreq);
	ifr = conf.ifc_req;

	for (i = 0; i < num; i ++)
	{
		struct sockaddr_in *sin = (struct sockaddr_in *)(&ifr->ifr_addr);
		if (ifr->ifr_addr.sa_family != AF_INET)
		{
			ifr ++;
			continue;
		}
		if (ioctl(socket_fd, SIOCGIFFLAGS, ifr) == 0 && (ifr->ifr_flags & IFF_LOOPBACK) == 0) // not loopback interface
		{
			if (g_onvif_cfg.server_ip[0] != '\0' && inet_addr(g_onvif_cfg.server_ip) != sin->sin_addr.s_addr)
			{
				// Only report the configured IP address, if <server_ip> is configured
				ifr ++;
				continue;
			}
			NetworkInterfaceList * p_net_inf = onvif_add_NetworkInterface(&g_onvif_cfg.network.interfaces);
			if (NULL == p_net_inf)
			{
				ifr ++;
				continue;
			}
			p_net_inf->NetworkInterface.Enabled = TRUE;
			p_net_inf->NetworkInterface.InfoFlag = 1;
			p_net_inf->NetworkInterface.Info.NameFlag = 1;
			p_net_inf->NetworkInterface.IPv4Flag = 1;
			p_net_inf->NetworkInterface.IPv4.Enabled = TRUE;
			
			//strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, inet_ntoa(sin->sin_addr));
			strcpy(p_net_inf->NetworkInterface.IPv4.Config.Address, g_onvif_cfg.server_ip);
			strncpy(p_net_inf->NetworkInterface.Info.Name, ifr->ifr_name, sizeof(p_net_inf->NetworkInterface.Info.Name) - 1);

			// get netmask
			if (ioctl(socket_fd, SIOCGIFNETMASK, ifr) == 0)
			{
				sin = (struct sockaddr_in *)(&ifr->ifr_netmask);
				p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = get_prefix_len_by_mask(inet_ntoa(sin->sin_addr));
			}
			else
			{
				p_net_inf->NetworkInterface.IPv4.Config.PrefixLength = 24;
			}

			// get dhcp
			NetworkConfigNew *pNet = (NetworkConfigNew *)getNetWorkConfig();
			if (pNet && ((pNet->wifiCfg.enable && pNet->wifiCfg.dhcpEnable) || pNet->lanCfg.dhcpEnable))
			{
				p_net_inf->NetworkInterface.IPv4.Config.DHCP = TRUE;
			}
			else
			{
				p_net_inf->NetworkInterface.IPv4.Config.DHCP = FALSE;
			}
			
			// get mtu
			if (ioctl(socket_fd, SIOCGIFMTU, ifr) == 0)
			{
				p_net_inf->NetworkInterface.Info.MTUFlag = 1;
				p_net_inf->NetworkInterface.Info.MTU = ifr->ifr_mtu;
			}

			// get hwaddr
			if (ioctl(socket_fd, SIOCGIFHWADDR, ifr) == 0) 
			{
				snprintf(p_net_inf->NetworkInterface.Info.HwAddress, sizeof(p_net_inf->NetworkInterface.Info.HwAddress), "%02X:%02X:%02X:%02X:%02X:%02X", 
					(uint8)ifr->ifr_hwaddr.sa_data[0], (uint8)ifr->ifr_hwaddr.sa_data[1], (uint8)ifr->ifr_hwaddr.sa_data[2],
					(uint8)ifr->ifr_hwaddr.sa_data[3], (uint8)ifr->ifr_hwaddr.sa_data[4], (uint8)ifr->ifr_hwaddr.sa_data[5]);
			}
		}
		ifr ++;
	}
	
	log_print(HT_LOG_INFO, "onvif_init_NetworkInterface OVER\n");
	closesocket(socket_fd);
	return;
}

void onvif_init_net()
{
	log_print(HT_LOG_INFO, "onvif_init_net START\n");

	g_onvif_cfg.network.DiscoveryModeFlag = 1;// default value is 1
	if (!g_onvif_cfg.network.DiscoveryModeFlag)
	{
		g_onvif_cfg.network.DiscoveryMode = DiscoveryMode_Discoverable;
	}  
	
	g_onvif_cfg.network.HostnameInformationFlag = 1;//
	if (!g_onvif_cfg.network.HostnameInformationFlag)
	{
		g_onvif_cfg.network.HostnameInformation.FromDHCP = FALSE;
		g_onvif_cfg.network.HostnameInformation.RebootNeeded = FALSE;
	}
	// init host name
	g_onvif_cfg.network.HostnameInformation.NameFlag = 1;
	gethostname(g_onvif_cfg.network.HostnameInformation.Name, sizeof(g_onvif_cfg.network.HostnameInformation.Name));
	if (!g_onvif_cfg.network.DNSInformationFlag)
	{
		const char * dns;
		// init dns setting
		g_onvif_cfg.network.DNSInformation.SearchDomainFlag = 1;
		g_onvif_cfg.network.DNSInformation.FromDHCP = FALSE;
		dns = get_dns_server();
		if (dns && strlen(dns) > 0)
			strncpy(g_onvif_cfg.network.DNSInformation.DNSServer[0], dns, sizeof(g_onvif_cfg.network.DNSInformation.DNSServer[0])-1);
		else
			strcpy(g_onvif_cfg.network.DNSInformation.DNSServer[0], "192.168.1.1");
		strcpy(g_onvif_cfg.network.DNSInformation.SearchDomain[0] , searchdomainname);
	}
	if (!g_onvif_cfg.network.NTPInformationFlag)
	{
		// init ntp settting
		g_onvif_cfg.network.NTPInformation.FromDHCP = FALSE;
		g_onvif_cfg.network.NTPInformation.IsExistIPV6Address = FALSE;
		char _NTPhostname[MID_INFO_LENGTH];
		GetNtpServer(_NTPhostname);
		strcpy(g_onvif_cfg.network.NTPInformation.NTPServer[0], _NTPhostname);
	}
	if (!g_onvif_cfg.network.NetworkProtocolFlag)
	{
		// init network protocol
		g_onvif_cfg.network.NetworkProtocol.HTTPFlag = 1;
		g_onvif_cfg.network.NetworkProtocol.HTTPEnabled = g_onvif_cfg.http_enable;
#ifdef HTTPS
		g_onvif_cfg.network.NetworkProtocol.HTTPSFlag = 1;
		g_onvif_cfg.network.NetworkProtocol.HTTPSEnabled = g_onvif_cfg.https_enable;
#endif
		g_onvif_cfg.network.NetworkProtocol.RTSPFlag = 1;
		g_onvif_cfg.network.NetworkProtocol.RTSPEnabled = 1;

		g_onvif_cfg.network.NetworkProtocol.HTTPPort[0] = g_onvif_cfg.http_port;
#ifdef HTTPS
		g_onvif_cfg.network.NetworkProtocol.HTTPSPort[0] = g_onvif_cfg.https_port;
#endif
		g_onvif_cfg.network.NetworkProtocol.RTSPPort[0] = 554;
	}
	if (!g_onvif_cfg.network.NetworkGatewayFlag)
	{
		const char * gw;
		// init default gateway
		gw = get_default_gateway();
		if (gw && strlen(gw) > 0)
			strncpy(g_onvif_cfg.network.NetworkGateway.IPv4Address[0], gw, sizeof(g_onvif_cfg.network.NetworkGateway.IPv4Address[0])-1);
		else
			strcpy(g_onvif_cfg.network.NetworkGateway.IPv4Address[0], "192.168.1.1");
	}
	// init network interface
	onvif_init_NetworkInterface();
	onvif_init_ZeroConfiguration();

	log_print(HT_LOG_INFO, "onvif_init_net OVER\n");
	return;
}

void onvif_init_def_cfg()
{
	memset(&g_onvif_cfg, 0, sizeof(ONVIF_CFG));
	memset(&g_onvif_cls, 0, sizeof(ONVIF_CLS));
	memset(&g_onvif_idx, 0, sizeof(ONVIF_IDX));

	//g_onvif_cfg.log_enable = 1; // logging
	//g_onvif_cfg.log_level = HT_LOG_ERR;

    if (gStreamCfg.webConfig.enable_onvif || gStreamCfg.webConfig.enable_web)
    {
        g_onvif_cfg.http_enable = 1;
    }
    else
    {
        g_onvif_cfg.http_enable = 0;
    }
	g_onvif_cfg.http_port = gStreamCfg.webConfig.webPort;

    g_onvif_cfg.http_max_users = HTTP_MAX_CLIENT_NUMS;

	g_onvif_cfg.evt_sim_flag = 0;
	g_onvif_cfg.evt_renew_time = 60;
	
	g_onvif_cfg.md5_hashing = 1;
	g_onvif_cfg.sha256_hashing = 0;

	return;
}

extern in_addr_t g_gateway_addr;
extern in_addr_t g_ip_addr;
extern char g_ifname[256];

void *wait_ipaddr_thr(void *arg)
{
	log_print(HT_LOG_INFO, "enter wait_ipaddr_thr\n");
	pthread_detach(pthread_self()); 

	in_addr_t gateway_addr;
	in_addr_t ip_addr;

	char gateway_addr_str[256]={0};
	char ip_addr_str[256]={0};

	int loopcount=0;
	char ifname[256]={0};

	while(1)
	{
		if (g_allnet_allocated == 1)
		{
			break;
		}
		if((loopcount%10) == 0)
		{
			log_print(HT_LOG_INFO, "wait ip address ready...\n");
		}
		
		if (Check_Link_Status(WIRE_INTERFACE_NAME)){
			strcpy(ifname,WIRE_INTERFACE_NAME);
		}
		else{
			if(Check_Link_Status(net_get_wireless_name()))
				strcpy(ifname,net_get_wireless_name());
			else
				strcpy(ifname,WIRE_INTERFACE_NAME);
		}

		ip_addr	   = net_get_ifaddr(ifname);
		gateway_addr = net_get_gateway();
		log_print(HT_LOG_DBG, "ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);

		if(ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF
					&& gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
		{
			get_ip_str(ip_addr, ip_addr_str, 256);
			get_ip_str(gateway_addr, gateway_addr_str, 256);
			log_print(HT_LOG_INFO, "ready! ip = 0x%x, %s, gateway = %s\n", ip_addr, ip_addr_str, gateway_addr_str);
			break;
		}
		loopcount++;
		sleep(1);
	}
	log_print(HT_LOG_INFO, "ip ready, wait_ipaddr_thr done\n");
	return NULL;
}

void *check_ipaddr_thr(void *arg)// IP monitor thread
{
	log_print(HT_LOG_INFO, "enter check_ipaddr_thr\n");
	pthread_detach(pthread_self()); 

	in_addr_t gateway_addr;
	in_addr_t ip_addr;

	//char gateway_addr_str[256] = {0};
	char ip_addr_str[256] = {0};
	char ifname[256] = {0};

	while(1)
	{
		if (g_allnet_allocated == 1)
		{
			break;
		}
		if (Check_Link_Status(WIRE_INTERFACE_NAME))
		{
			strcpy(ifname, WIRE_INTERFACE_NAME);
		}
		else
		{
			if(Check_Link_Status(net_get_wireless_name()))
				strcpy(ifname, net_get_wireless_name());
			else
				strcpy(ifname, WIRE_INTERFACE_NAME);
		}

		ip_addr      = net_get_ifaddr(ifname);
		gateway_addr = net_get_gateway();
		log_print(HT_LOG_DBG, "ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);
		if ((ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF) &&
			((ip_addr != g_ip_addr) ||
			 ((gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF) && (gateway_addr != g_gateway_addr))))
		{
			log_print(HT_LOG_WARN, "onvif ip/gateway changed, ip=0x%x gateway=0x%x\n", ip_addr, gateway_addr);
			g_ip_addr = ip_addr;
			if (gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
			{
				g_gateway_addr = gateway_addr;
			}
			if (1)
			{
				get_ip_str(ip_addr, ip_addr_str, 256);
				log_print(HT_LOG_INFO, "ip_addr_str:%s\n", ip_addr_str);
				if (1)
				{
					onvif_stop_discovery();
				}
				if (1)
				{
					// Update onvif service IP address
					snprintf(g_onvif_cls.server_ip,
					         sizeof(g_onvif_cls.server_ip),
					         "%.*s",
					         (int)(sizeof(g_onvif_cls.server_ip) - 1),
					         ip_addr_str);
					
					snprintf(g_onvif_cfg.server_ip,
					         sizeof(g_onvif_cfg.server_ip),
					         "%.*s",
					         (int)(sizeof(g_onvif_cfg.server_ip) - 1),
					         ip_addr_str);
				}
				
				// Update device capability set and onvif service address
				onvif_init_capabilities();

				if (1)
				{
					http_srv_deinit(&g_onvif_cls.http_srv);
					if (!http_srv_init(&g_onvif_cls.http_srv, NULL, g_onvif_cls.http_port, g_onvif_cfg.http_max_users, 0, NULL, NULL))
					{
						log_print(HT_LOG_ERR, "http server listen on http://%s:%u failed\r\n", g_onvif_cfg.server_ip, g_onvif_cls.http_port);
					}
					else
					{
						http_set_msg_cb(&g_onvif_cls.http_srv, onvif_http_msg_cb, NULL);
						http_set_data_cb(&g_onvif_cls.http_srv, onvif_http_data_cb, NULL);
#ifdef IPFILTER_SUPPORT
						http_set_conn_cb(&g_onvif_cls.http_srv, onvif_http_conn_cb, NULL);
#endif
						log_print(HT_LOG_INFO, "Onvif server running at http://%s:%u\n", g_onvif_cls.server_ip, g_onvif_cls.http_port);
					}
				}
				if (1)// restart discovery announce
				{
					onvif_start_discovery();
					onvif_hello();
				}
			}
		}
		sleep(5);
	}
	
	log_print(HT_LOG_INFO, "restart webserver_easy\n");
	
	//exit(0);
	
	return NULL;
}

void onvif_detect_ip()
{
	in_addr_t gateway_addr = INADDR_ANY;
	in_addr_t ip_addr = INADDR_ANY;
	int loopcount = 0;
	char gateway_addr_str[256];
	char ip_addr_str[256];
	char ifname[256];

	log_print(HT_LOG_INFO, "IP INFORMATION\n");

	while (1)
	{
		if ((loopcount % 10) == 0)
			log_print(HT_LOG_INFO, "wait ip address ready...\n");
		if (loopcount++ == 60)
			break;
		if (Check_Link_Status(WIRE_INTERFACE_NAME))
		{
			strcpy(ifname, WIRE_INTERFACE_NAME);
		}
		else
		{
			if (Check_Link_Status(net_get_wireless_name()))
				strcpy(ifname, net_get_wireless_name());
			else
				strcpy(ifname, WIRE_INTERFACE_NAME);
		}
		ip_addr = net_get_ifaddr(ifname);
		gateway_addr = net_get_gateway();
		log_print(HT_LOG_INFO, "ifname = %s, ip = 0x%x, gateway = 0x%x\n", ifname, ip_addr, gateway_addr);
		if (ip_addr != INADDR_ANY && ip_addr != 0xFFFFFFFF && gateway_addr != INADDR_ANY && gateway_addr != 0xFFFFFFFF)
		{
			get_ip_str(ip_addr, ip_addr_str, 256);
			get_ip_str(gateway_addr, gateway_addr_str, 256);
			log_print(HT_LOG_INFO, "ip = 0x%x, %s, gateway = %s\n", ip_addr, ip_addr_str, gateway_addr_str);
			break;
		}
		
		sleep(1);
	}
	// timeout handling
	if(loopcount >= 60)
	{ 
		pthread_t tid = 0;
		anj_thread_create(&tid, 0, "wait_ipaddr", (void *(*)(void *))wait_ipaddr_thr, NULL, 1);
	}

	get_ip_str(ip_addr, ip_addr_str, 256);
	get_ip_str(gateway_addr, gateway_addr_str, 256);
	strcpy(g_ifname, ifname);
	g_ip_addr = ip_addr;
	g_gateway_addr = gateway_addr;
	snprintf(g_onvif_cfg.server_ip,
	         sizeof(g_onvif_cfg.server_ip),
	         "%.*s",
	         (int)(sizeof(g_onvif_cfg.server_ip) - 1),
	         ip_addr_str);
	log_print(HT_LOG_INFO, "g_onvif_cfg.server_ip:%s\n", g_onvif_cfg.server_ip);
	// IP monitor
	{
		pthread_t tid = 0;
		anj_thread_create(&tid, 0, "check_ipaddr", (void *(*)(void *))check_ipaddr_thr, NULL, 1);
	}
}

void onvif_init_nvrip()
{
#if ONVIF_ALL_NET
	char ifname[256];
	char ip_addr_str[256];

	strcpy(ifname, g_ifname);
	get_ip_str(g_ip_addr, ip_addr_str, 256);

	LANConfig *pLan = (LANConfig *)getNetWorkConfig();
	if (pLan && pLan->onvifAllnetEnable)
	{
		read_saved_nvr_ip();
		if(strlen(ip_addr_str) > 0)
		{
			// Check whether NVR_IP[] entries are online and clear offline entries.
			int j = 0;
			for(j = 0; j < 10; j ++)
			{
				if(strlen(NVR_IP[j]) > 0)
				{
					char *if_name = ifname; // network interface, e.g. eth0
					char *str_src_ip = ip_addr_str; // local device IP
					char *str_dst_ip = NVR_IP[j];
					unsigned char dst_mac[ARP_MAC_BYTE];
					int ret = arp_get_mac(if_name, str_src_ip, str_dst_ip, dst_mac, ARP_TIME_OUT_MS);
					if(ret>0)
					{ // NVR is online
						log_print(HT_LOG_INFO, "ret=%d,NVR_IP[%d](%s) on line!\n", ret, j, str_dst_ip);
					}
					else
					{
						log_print(HT_LOG_INFO, "ret=%d,NVR_IP[%d](%s) NOT on line!\n", ret, j, str_dst_ip);
						memset(NVR_IP[j], 0, sizeof(NVR_IP[j]));
					}
				}
			}
			write_saved_nvr_ip();
			read_saved_nvr_ip();
		}
	}
#endif
}

void onvif_init_http()
{
#ifdef HTTPS
	g_onvif_cls.https_port = get_https_port();
	g_onvif_cfg.https_enable = 1;
	
	// Select cert/key files.
	if((access(SERVER_CERT_FILE_UPLOAD, F_OK) == 0)  && (access(SERVER_PRIVATE_KEY_FILE_UPLOAD, F_OK) == 0))
	{
		strncpy(g_onvif_cfg.cert_file, SERVER_CERT_FILE_UPLOAD, sizeof(g_onvif_cfg.cert_file) - 1);
		strncpy(g_onvif_cfg.key_file, SERVER_PRIVATE_KEY_FILE_UPLOAD, sizeof(g_onvif_cfg.key_file) - 1);
	}
	else
	{
		strncpy(g_onvif_cfg.cert_file, SERVER_CERT_FILE, sizeof(g_onvif_cfg.cert_file) - 1);
		strncpy(g_onvif_cfg.key_file, SERVER_PRIVATE_KEY_FILE, sizeof(g_onvif_cfg.key_file) - 1);
	}
#endif
}

void onvif_init_sn_uuid()
{
	// sn+uuid
	onvif_get_sn(g_SN);
	onvif_get_uuid(g_uuid);
	log_print(HT_LOG_INFO, "g_SN:%s, g_uuid:%s\n", g_SN, g_uuid);
}

void onvif_init_media()
{
#if defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
	log_print(HT_LOG_INFO, "onvif_init_media START\n");
	// thirdstream_enable = (gMediaCfg.videoConfig[0].videoEncode.encodeCfg[2].enable);
	thirdstream_enable = 0;
	onvif_init_VideoSource();
	//////////////////////////////////trt--Max
	onvif_init_VideoSourceConfiguration();
	
	onvif_init_VideoEncoderConfiguration();
	//////////////////////////////////video_encoder_config

#ifdef MEDIA2_SUPPORT
	log_print(HT_LOG_INFO, "onvif_init_media MEDIA2_SUPPORT\n");
	onvif_init_MaskOptions();
	onvif_init_Masks();
	log_print(HT_LOG_INFO, "onvif_init_media MEDIA2_SUPPORT OVER\n");
#endif

#ifdef AUDIO_SUPPORT
	log_print(HT_LOG_INFO, "onvif_init_media AUDIO_SUPPORT\n");
	onvif_init_AudioSource();
	onvif_init_AudioSourceConfiguration();
	onvif_init_AudioEncoderConfiguration();
	onvif_init_AudioDecoderConfigurations();
	log_print(HT_LOG_INFO, "onvif_init_media AUDIO_SUPPORT OVER\n");
#endif

	onvif_init_MetadataConfiguration();
	onvif_init_MetadataConfigurationOptions();

	onvif_init_OSDConfigurations();
	onvif_init_OSDConfigurationOptions();

#ifdef VIDEO_ANALYTICS
	log_print(HT_LOG_INFO, "onvif_init_media VIDEO_ANALYTICS_SUPPORT\n");
	onvif_init_VideoAnalyticsConfiguration();
	if (g_onvif_cfg.va_cfg)
	{
		onvif_init_SupportedRules(&g_onvif_cfg.va_cfg->SupportedRules);
		onvif_init_SupportedAnalyticsModules(&g_onvif_cfg.va_cfg->SupportedAnalyticsModules);
	}
	log_print(HT_LOG_INFO, "onvif_init_media VIDEO_ANALYTICS_SUPPORT OVER\n");
#endif
	log_print(HT_LOG_INFO, "onvif_init_media OVER\n");
#endif // defined(MEDIA_SUPPORT) || defined(MEDIA2_SUPPORT)
}

void onvif_init_ptz()
{
#ifdef PTZ_SUPPORT
	ptz_tour_init();
	onvif_init_PTZNode();
	onvif_init_PTZConfiguration();
#endif
}

void onvif_init_cfg()
{
	log_print(HT_LOG_INFO, "onvif_init_cfg START\n");

	if (gStreamCfg.webConfig.onvif_auth == 1)
		g_onvif_cfg.need_auth = 1;
	
	onvif_detect_ip();
	onvif_init_nvrip();
	if (g_onvif_cfg.http_port > 0 && g_onvif_cfg.http_port < 65535) 
	{
		log_print(HT_LOG_INFO, "WEB PORT INFORMATION\n");
		g_onvif_cls.http_port = g_onvif_cfg.http_port;
		if (g_onvif_cls.http_port <= 0 || g_onvif_cls.http_port > 65535)
		{
			g_onvif_cls.http_port = 80;
		}
		g_onvif_cfg.http_enable = 1;
		
		log_print(HT_LOG_INFO, "http_port:%d, https_enable:%d, http_enable:%d\n", g_onvif_cls.http_port, g_onvif_cfg.https_enable, g_onvif_cfg.http_enable);
	}

	// HTTP
	onvif_init_http();
	// SN+UUID
	onvif_init_sn_uuid();
	// username+password
	onvif_init_User();
	// DeviceInformation
	onvif_init_DeviceInformation();
	// system,dateandtime
	onvif_init_SystemDateTime();
	// Scope
	onvif_init_Scope();

	if (g_onvif_cfg.EndpointReference[0] == 0)
	{
		snprintf(g_onvif_cfg.EndpointReference,
		         sizeof(g_onvif_cfg.EndpointReference),
		         "%s",
		         g_uuid);
	}

	// Media (media + media2 + audio + analytics)
	onvif_init_media();
	// PTZ
	onvif_init_ptz();

	// onvif_init_profile
	onvif_init_profile();

#ifdef PROFILE_C_SUPPORT
	onvif_init_DoorList();
	onvif_init_AreaList();
	onvif_init_AccessPointList();
#endif

#ifdef DEVICEIO_SUPPORT
#ifdef AUDIO_SUPPORT
	// If do not support rtsp back channel, you can comment it out
	onvif_init_AudioOutput();
	onvif_init_AudioOutputConfiguration(g_onvif_cfg.a_output);
#endif
	onvif_init_RelayOutput();
	onvif_init_DigitInput();
	onvif_init_SerialPort();
#endif

#ifdef PROFILE_G_SUPPORT
	onvif_init_Recording();
	onvif_init_RecordingJob();

	g_onvif_cfg.replay_session_timeout = 60;
#endif

#ifdef ACCESS_RULES
	onvif_init_AccessProfile();
#endif

#ifdef CREDENTIAL_SUPPORT
	onvif_init_Credential();
#endif

#ifdef SCHEDULE_SUPPORT
	onvif_init_Schedule();
#endif

	log_print(HT_LOG_INFO, "onvif_init_cfg OVER\n");

	return;
}

void onvif_chk_server_cfg()
{
	if (g_onvif_cfg.server_ip[0] == '\0')
	{
		strcpy(g_onvif_cls.server_ip, get_local_ip());
	}
	else
	{
		strcpy(g_onvif_cls.server_ip, g_onvif_cfg.server_ip);
	}

	if (g_onvif_cfg.http_enable)
	{
		g_onvif_cls.http_port = g_onvif_cfg.http_port;
		if (g_onvif_cls.http_port <= 0 || g_onvif_cls.http_port > 65535) 
		{
			g_onvif_cls.http_port = 80;
		}
	}
#ifdef HTTPS
	if (g_onvif_cfg.https_enable)
	{
		g_onvif_cls.https_port = g_onvif_cfg.https_port;
		if (g_onvif_cls.https_port <= 0 || g_onvif_cls.https_port > 65535) 
		{
			g_onvif_cls.https_port = 443;
		}
	}
#endif
	if (g_onvif_cfg.evt_renew_time < 60)
	{
		g_onvif_cfg.evt_renew_time = 60;
	}

	return;
}

HT_API void onvif_init()
{
	ONVIF_PROFILE * p_profile;

	onvif_chk_server_cfg();

	onvif_init_net();	// initialize network section

	onvif_eua_init();

	p_profile = g_onvif_cfg.profiles;
	while (p_profile)
	{
#ifdef PTZ_SUPPORT	
		if (NULL == p_profile->ptz_cfg && g_onvif_cfg.ptz_cfg)
		{
			// add PTZ configuration to profile
			p_profile->ptz_cfg = g_onvif_cfg.ptz_cfg;
			p_profile->ptz_cfg->Configuration.UseCount++;
		}
#endif
		if (NULL == p_profile->metadata_cfg && g_onvif_cfg.metadata_cfg)
		{
			// add metadata configuration to profile
			p_profile->metadata_cfg = g_onvif_cfg.metadata_cfg;
			p_profile->metadata_cfg->Configuration.UseCount++;
		}
#ifdef VIDEO_ANALYTICS
		if (NULL == p_profile->va_cfg && g_onvif_cfg.va_cfg)
		{
			// add video analytics configuration to profile
			p_profile->va_cfg = g_onvif_cfg.va_cfg;
			p_profile->va_cfg->Configuration.UseCount++;
		}
#endif
		p_profile = p_profile->next;
	}

	onvif_init_capabilities();	// initialize capabilities

	return;
}




