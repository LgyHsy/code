#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_log.h"
#include "anj_mw_str.h"
#include "anj_comm.h"
#include "anj_config.h"


int anj_config_platform_get(IXML_Node *pNode, PlatformConfig* pPlatformCfg)
{
    if (NULL == pNode || NULL == pPlatformCfg)
    {
        __ERR("error: param is null\n");
        return -1;
    }

	memset(pPlatformCfg, 0, sizeof(PlatformConfig));

	IXML_Node* tmpChild = pNode->firstChild;
	IXML_Node* tmpAttr = NULL;
	
	//default value if not found
	pPlatformCfg->vmCfg.enable = 0;
	pPlatformCfg->voipCfg.PlayTone = 0;
	pPlatformCfg->voipCfg.InRingTimes = 1;      // 默认被叫时振铃一次   
	pPlatformCfg->voipCfg.reserved = 1;             // 韩国门铃，默认开启广播信息
	pPlatformCfg->voipCfg.keyPressTimeLen = 500;    // 默认按键500ms才触发呼叫

	while(tmpChild != NULL)
	{
		if(NULL != strstr(tmpChild->nodeName, "Config") )
		{		
			tmpAttr = tmpChild->firstAttr;
			while(tmpAttr != NULL)
			{
				if(!strcmp(tmpAttr->nodeName, "Enable"))
				{
					pPlatformCfg->vmCfg.enable = Str2Num(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "Server"))
				{
					StrCpy(pPlatformCfg->vmCfg.server, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "Port"))
				{
					pPlatformCfg->vmCfg.port = CheckAtoU(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName,"Username"))
				{
					StrCpy(pPlatformCfg->vmCfg.username, ACCOUNT_NAME_MAX_LEN, tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName,"Password"))
				{
					StrCpy(pPlatformCfg->vmCfg.password, ACCOUNT_PASSWORD_MAX_LEN, tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "EncryptPwd"))
				{
					memset(pPlatformCfg->vmCfg.password, '\0', ACCOUNT_PASSWORD_MAX_LEN);

					char szEncryptData[64] = {0};
					StrCpy(szEncryptData, 64, tmpAttr->nodeValue);

					char dst[32] = {0};
					int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
					if( ret != 0 )
					{
						__ERR("decrypt %s error.\n", szEncryptData);
					}
					else
					{
                        char *tmpDst = restore_with_escape(dst);
                        if (tmpDst != NULL)
                        {
                            StrCpy(pPlatformCfg->vmCfg.password, ACCOUNT_PASSWORD_MAX_LEN, tmpDst);
                            free(tmpDst);
                            tmpDst = NULL;
                        }
                        else
                        {
                            pPlatformCfg->vmCfg.password[0] = '\0';
                        }
					}
				}		

// VOIP CONFIG				###################
				else if(!strcmp(tmpAttr->nodeName, "PlayTone"))
				{
					pPlatformCfg->voipCfg.PlayTone = Str2Num(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "InRingTimes"))
				{
					pPlatformCfg->voipCfg.InRingTimes = Str2Num(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "reserved"))
				{
					pPlatformCfg->voipCfg.reserved = Str2Num(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "keyPressTimeLen"))
				{
					pPlatformCfg->voipCfg.keyPressTimeLen = Str2Num(tmpAttr->nodeValue);
				}
				else if(!strcmp(tmpAttr->nodeName, "FileName"))
				{
					memset(pPlatformCfg->voipCfg.filename, '\0', MAX_IPC_FILENAME_LEN);
					StrCpy(pPlatformCfg->voipCfg.filename, MAX_IPC_FILENAME_LEN, tmpAttr->nodeValue);
				}

				tmpAttr = tmpAttr->nextSibling;				
			}
			
			break;
		}

		tmpChild = tmpChild->nextSibling;
	}

	return 0;
}

char *anj_config_platform_conver_xml(PlatformConfig * pPlatformCfg, int bPwdEntrypt)
{
	int maxSize = 2048;
	char *pe = NULL;
	char *pb = NULL;
	char *buf = NULL;
	char escapeBuf[128] = {0};

	buf = (char*)anj_mw_malloc(maxSize);
	pb = buf;
	pe = buf + maxSize -1;

	pb += snprintf(pb, pe-pb, "<PlatformConfig>\r\n");

	pb += snprintf(pb, pe-pb, "<TopseeConfig\r\n");
	pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n",	  pPlatformCfg->vmCfg.enable);
	pb += snprintf(pb, pe-pb, "Server=\"%s\"\r\n",	  pPlatformCfg->vmCfg.server);
	pb += snprintf(pb, pe-pb, "Port=\"%u\"\r\n",		pPlatformCfg->vmCfg.port);
	pb += snprintf(pb, pe-pb, "Username=\"%s\"\r\n", pPlatformCfg->vmCfg.username);

	copy_with_escape(escapeBuf, pPlatformCfg->vmCfg.password);

	char dst[64] = {0};	
	int ret = StringEncrypt(escapeBuf, dst, sizeof(dst) );
	if( 0 == bPwdEntrypt || ret != 0 )
	{
		if(ret != 0)
		{
		    __ERR("Encrypt %s error.", escapeBuf);
		}
		pb += snprintf(pb, pe-pb, "Password=\"%s\"\r\n", escapeBuf);
	}
	else
	{
		pb += snprintf(pb, pe-pb, "EncryptPwd=\"%s\"\r\n", dst);
	}
	
	pb += snprintf(pb, pe-pb, "PlayTone=\"%d\"\r\n", pPlatformCfg->voipCfg.PlayTone);
	pb += snprintf(pb, pe-pb, "InRingTimes=\"%d\"\r\n", pPlatformCfg->voipCfg.InRingTimes);
	pb += snprintf(pb, pe-pb, "reserved=\"%d\"\r\n", pPlatformCfg->voipCfg.reserved);
	pb += snprintf(pb, pe-pb, "keyPressTimeLen=\"%d\"\r\n", pPlatformCfg->voipCfg.keyPressTimeLen);
	pb += snprintf(pb, pe-pb, "FileName=\"%s\"\r\n", pPlatformCfg->voipCfg.filename);

	PlatRegResult regResult;
	memset(&regResult, 0, sizeof(regResult));

	int fd = open(PLATFORM_REGISGER_RESULT_FILE, O_RDONLY);
	if(fd >= 0)
	{
		int	readCnt = read(fd, &regResult, sizeof(regResult));
		if(readCnt == sizeof(regResult))
		{
			pb += snprintf(pb, pe-pb, "RegResult=\"%d\"\r\n", regResult.result);
			pb += snprintf(pb, pe-pb, "PlatDevID=\"%s\"\r\n", regResult.szPlatDevId);
			pb += snprintf(pb, pe-pb, "reservedInfo=\"%s\"\r\n", regResult.reservedInfo);
		}
		else
		{
			__ERR("read failed.\n");
		}
		close(fd);
	}

	pb += snprintf(pb, pe-pb, "/>\r\n"); 
	pb += snprintf(pb, pe-pb, "</PlatformConfig>\r\n");

	return buf;
}



int anj_config_platform_save(PlatformConfig *pPlatformCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_platform_conver_xml(pPlatformCfg, 1);
    iRet = anj_config_save_node(pDataXml, "<PlatformConfig>", "</PlatformConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_platform_set(PlatformConfig *pPlatformCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    PlatformConfig *pOldPlatformCfg = (PlatformConfig *)getPlatformConfig();
    if (memcmp(pOldPlatformCfg, pPlatformCfg, sizeof(PlatformConfig)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pOldPlatformCfg, pPlatformCfg, sizeof(PlatformConfig));
        anj_config_platform_save(pOldPlatformCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}


int anj_config_platform_get_by_xml(PlatformConfig *pPlatformCfg, char *xmlBuf)
{
    if (NULL == pPlatformCfg || NULL == xmlBuf)
    {
        __ERR("error:param is null\n");
        return -1;
    }

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if(pDocNode == NULL)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "PlatformConfig");
	if(pNodelist != NULL)
	{	
		anj_config_platform_get(pNodelist->nodeItem, pPlatformCfg);
		ixmlNodeList_free(pNodelist);
		ixmlDocument_free(pDocNode);
	}
	else
	{
		ixmlDocument_free(pDocNode);			
		return -1;
	}

	return 0;
}

int anj_config_platform_load(PlatformConfig *pPlatformCfg)
{
    return anj_config_load("PlatformConfig", pPlatformCfg, 0);
}

