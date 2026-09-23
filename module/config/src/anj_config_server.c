#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_log.h"
#include "anj_mw_str.h"
#include "anj_comm.h"
#include "anj_config.h"


static int anj_config_server_ftp_get(IXML_Node *pNode, FtpServer *pFtpServer)
{
	IXML_Node* tmpAttr = NULL;
	tmpAttr = pNode->firstAttr;

	while(tmpAttr)
	{
		if(!strcmp(tmpAttr->nodeName, "Index"))
		{		
			pFtpServer->index = Str2Num(tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "ServerIP"))
		{
			memset(pFtpServer->serverIP, '\0', MAX_IP_NAME_LEN);
			StrCpy(pFtpServer->serverIP, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "ServerPort"))
		{
			pFtpServer->serverPort = Str2Num(tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "Username"))
		{
			memset(pFtpServer->userName, '\0', FTP_NAME_MAX_LEN);
			StrCpy(pFtpServer->userName, FTP_NAME_MAX_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "Password"))
		{
			memset(pFtpServer->password, '\0', FTP_PASSWORD_MAX_LEN);
			StrCpy(pFtpServer->password, FTP_PASSWORD_MAX_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "EncryptPwd"))
		{
			memset(pFtpServer->password, '\0', FTP_PASSWORD_MAX_LEN);
			char szEncryptData[64] = {0};
			StrCpy(szEncryptData, 64, tmpAttr->nodeValue);

			char dst[32] = {0};
			int ret = StringDecrypt(szEncryptData, dst, sizeof(dst) );
			if( ret != 0 )
			{
				__ERR("decrypt %s error.\n", szEncryptData);
			}
			else
			{
			    char *tmpDst = restore_with_escape(dst);
			    if (tmpDst != NULL)
			    {
				    StrCpy(pFtpServer->password, FTP_PASSWORD_MAX_LEN, tmpDst);
				    free(tmpDst);
				    tmpDst = NULL;
				}
				else
				{
                    pFtpServer->password[0] = '\0';
				}
			}
		}	
		else if(!strcmp(tmpAttr->nodeName, "FilePath"))
		{
			memset(pFtpServer->filePath, '\0', FTP_PATH_MAX_LEN);
			StrCpy(pFtpServer->filePath, FTP_PATH_MAX_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "FileSize"))
		{
			pFtpServer->fileSize = Str2Num(tmpAttr->nodeValue);
		}

		tmpAttr = tmpAttr->nextSibling;
	}

	return 0;	
}

static int anj_config_server_smtp_server_get(IXML_Node *pNode, SmtpServer *pSmtpServer)
{
	IXML_Node* tmpAttr = NULL;
	
	tmpAttr = pNode->firstAttr;
	while(tmpAttr)
	{
		if(!strcmp(tmpAttr->nodeName, "Index"))
		{		
			pSmtpServer->index = Str2Num(tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "ToEmail"))
		{
			memset(pSmtpServer->toMail, '\0', SMTP_ACCOUNT_MAX_LEN);
			StrCpy(pSmtpServer->toMail, SMTP_ACCOUNT_MAX_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "CcEmail"))
		{
			memset(pSmtpServer->ccMail, '\0', SMTP_ACCOUNT_MAX_LEN);
			StrCpy(pSmtpServer->ccMail, SMTP_ACCOUNT_MAX_LEN, tmpAttr->nodeValue);
		}
		else if(!strcmp(tmpAttr->nodeName, "Subject"))
		{
			memset(pSmtpServer->subject, '\0', SMTP_SUBJECT_MAX_LEN);
			StrCpy(pSmtpServer->subject, SMTP_SUBJECT_MAX_LEN, tmpAttr->nodeValue);
		}

		tmpAttr = tmpAttr->nextSibling;
	}

	return 0;
}


static int anj_config_server_smtp_list_get(IXML_Node *pNode, SmtpServerList *pSmtpList)
{
	IXML_Node *childNode = pNode->firstChild;

	memset(pSmtpList->smtpServers, 0, sizeof(SmtpServer)*SMTP_SERVER_COUNT);

	int curIdx = 0;
	while(childNode)
	{
		if(curIdx >= SMTP_SERVER_COUNT)
		{
			break;
		}

		anj_config_server_smtp_server_get(childNode, &(pSmtpList->smtpServers[curIdx++]));
		childNode = childNode->nextSibling;
	}

	if(curIdx != SMTP_SERVER_COUNT) //total 4 smtpServers
	{
		__ERR("smtp server count=%d, old should be %d\n", curIdx, SMTP_SERVER_COUNT);
	}

	IXML_Node *attrNode = pNode->firstAttr;
	while(attrNode)
	{
		if(!strcmp(attrNode->nodeName, "ServerIP"))
		{
			memset(pSmtpList->serverIP, '\0', MAX_IP_NAME_LEN);
			StrCpy(pSmtpList->serverIP, MAX_IP_NAME_LEN, attrNode->nodeValue);
		}
		else if(!strcmp(attrNode->nodeName, "ServerPort"))
		{
			pSmtpList->serverPort = Str2Num(attrNode->nodeValue);
		}
		else if(!strcmp(attrNode->nodeName, "Auth"))
		{
			pSmtpList->auth = Str2Num(attrNode->nodeValue);
		}
		else if(!strcmp(attrNode->nodeName, "Username"))
		{
			memset(pSmtpList->userName, '\0', SMTP_NAME_MAX_LEN);
			StrCpy(pSmtpList->userName, SMTP_NAME_MAX_LEN, attrNode->nodeValue);
		}
		else if(!strcmp(attrNode->nodeName, "Password"))
		{
			memset(pSmtpList->password, '\0', SMTP_PASSWORD_MAX_LEN);
			StrCpy(pSmtpList->password, SMTP_PASSWORD_MAX_LEN, attrNode->nodeValue);

		}
		else if(!strcmp(attrNode->nodeName, "EncryptPwd"))
		{
			memset(pSmtpList->password, '\0', SMTP_PASSWORD_MAX_LEN);
			char szEncryptData[64] = {0};
			StrCpy(szEncryptData, 64, attrNode->nodeValue);

			char dst[32] = {0};
			int ret = StringDecrypt(szEncryptData, dst, sizeof(dst) );
			if( ret != 0 )
			{
				__ERR("decrypt %s error.\n", szEncryptData);
			}
			else
			{
			    char *tmpDst = restore_with_escape(dst);
			    if (tmpDst != NULL)
			    {
				    StrCpy(pSmtpList->password, SMTP_PASSWORD_MAX_LEN, tmpDst);
				    free(tmpDst);
				    tmpDst = NULL;
				}
				else
				{
                    pSmtpList->password[0] = '\0';
				}
			}
		}		
		else if(!strcmp(attrNode->nodeName, "FromEmail"))
		{
			memset(pSmtpList->fromMail, '\0', SMTP_ACCOUNT_MAX_LEN);
			StrCpy(pSmtpList->fromMail, SMTP_ACCOUNT_MAX_LEN, attrNode->nodeValue);
		}

		attrNode = attrNode->nextSibling;
	}

	return 0;
}

int anj_config_server_get(IXML_Node *pNode, ServerConfig* pServerCfg)
{
    if (NULL == pNode || NULL == pServerCfg)
    {
        return -1;
    }

    memset(pServerCfg, 0, sizeof(ServerConfig));

    IXML_Node* pChildNode = pNode->firstChild;

	while(pChildNode != NULL)
	{
		if(!strcmp(pChildNode->nodeName, "FTPList"))
		{
			IXML_Node *childNode = pChildNode->firstChild;
			int curIdx = 0;
			while(childNode)
			{
				if(curIdx >= FTP_SERVER_COUNT)
				{
					break;
				}

				anj_config_server_ftp_get(childNode, &(pServerCfg->ftpServers[curIdx++]));
				childNode = childNode->nextSibling;
			}
	
			if(curIdx != FTP_SERVER_COUNT) //total 6 ftpservers
			{
				__ERR( "xml error, ftp count should be %d, not %d\n", FTP_SERVER_COUNT, curIdx);
			}
		}
		else if(!strcmp(pChildNode->nodeName, "SMTPList"))
		{
			anj_config_server_smtp_list_get(pChildNode, &(pServerCfg->smtpServers));
		}			

		pChildNode = pChildNode->nextSibling;
	}

    return 0;
}

char *anj_config_server_ftp_conver_xml(FtpServerList *pFtpServerList, int bPwdEntrypt)
{
	int maxSize = 1000;
	char *pe = NULL;
	char *pb = NULL;
	char *buf = NULL;
	int i = 0;
	char escapeBuf[1000] = {0};

	buf = (char*)anj_mw_malloc(maxSize);
	pb = buf;
	pe = buf + maxSize -1;

	pb += snprintf(pb, pe-pb, "<FTPList>\r\n");

	for(i = 0; i < FTP_SERVER_COUNT; i++)
	{
		pb += snprintf(pb, pe-pb, "<FTPConfig\r\n");
		pb += snprintf(pb, pe-pb, "Index=\"%d\"\r\n", pFtpServerList->ftpServers[i].index);
		pb += snprintf(pb, pe-pb, "ServerIP=\"%s\"\r\n", copy_with_escape(escapeBuf,pFtpServerList->ftpServers[i].serverIP));
		pb += snprintf(pb, pe-pb, "ServerPort=\"%d\"\r\n", pFtpServerList->ftpServers[i].serverPort);
		pb += snprintf(pb, pe-pb, "Username=\"%s\"\r\n", copy_with_escape(escapeBuf,pFtpServerList->ftpServers[i].userName));

		copy_with_escape(escapeBuf,pFtpServerList->ftpServers[i].password);

		char dst[64] = {0}; 
		int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
		if( 0 == bPwdEntrypt || ret != 0 )
		{
			if( ret != 0)__ERR("Encrypt %s error.", escapeBuf);
			pb += snprintf(pb, pe-pb, "Password=\"%s\"\r\n", escapeBuf);
		}
		else
		{
			pb += snprintf(pb, pe-pb, "EncryptPwd=\"%s\"\r\n", dst);
		}

		pb += snprintf(pb, pe-pb, "FilePath=\"%s\"\r\n", copy_with_escape(escapeBuf, pFtpServerList->ftpServers[i].filePath));
		pb += snprintf(pb, pe-pb, "FileSize=\"%d\"\r\n",pFtpServerList->ftpServers[i].fileSize);
		pb += snprintf(pb, pe-pb, "/>\r\n");
	}


	pb += snprintf(pb, pe-pb, "</FTPList>\r\n");

	return buf;
}

char *anj_config_server_smtp_conver_xml(SmtpServerList *pSmtpServerList, int bPwdEntrypt)
{
	int maxSize = 1000;
	char *pe = NULL;
	char *pb = NULL;
	char *buf = NULL;
	int i = 0;
	char escapeBuf[1000] = {0};

	buf = (char*)anj_mw_malloc(maxSize);
	pb = buf;
	pe = buf + maxSize -1;

	pb += snprintf(pb, pe-pb, "<SMTPList\r\n");
	pb += snprintf(pb, pe-pb, "ServerIP=\"%s\"\r\n", copy_with_escape(escapeBuf,pSmtpServerList->serverIP));
	pb += snprintf(pb, pe-pb, "ServerPort=\"%d\"\r\n", pSmtpServerList->serverPort);
	pb += snprintf(pb, pe-pb, "Auth=\"%d\"\r\n", pSmtpServerList->auth);
	pb += snprintf(pb, pe-pb, "Username=\"%s\"\r\n", copy_with_escape(escapeBuf,pSmtpServerList->userName));

	copy_with_escape(escapeBuf,pSmtpServerList->password);

	char dst[64] = {0}; 
	int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
	if( 0 == bPwdEntrypt || ret != 0 )
	{
		if( ret != 0)__ERR("Encrypt %s error.", escapeBuf);
		pb += snprintf(pb, pe-pb, "Password=\"%s\"\r\n", escapeBuf);
	}
	else
	{
		pb += snprintf(pb, pe-pb, "EncryptPwd=\"%s\"\r\n", dst);
	}


	pb += snprintf(pb, pe-pb, "FromEmail=\"%s\"\r\n", copy_with_escape(escapeBuf,pSmtpServerList->fromMail));
	pb += snprintf(pb, pe-pb, ">\r\n");
	

	for(i = 0; i < SMTP_SERVER_COUNT; i++)
	{
		pb += snprintf(pb, pe-pb, "<SMTPConfig\r\n");
		pb += snprintf(pb, pe-pb, "Index=\"%d\"\r\n", i);//pSmtpServerList->smtpServers[i].index);
		pb += snprintf(pb, pe-pb, "ToEmail=\"%s\"\r\n", copy_with_escape(escapeBuf,pSmtpServerList->smtpServers[i].toMail));
		pb += snprintf(pb, pe-pb, "CcEmail=\"%s\"\r\n", copy_with_escape(escapeBuf,pSmtpServerList->smtpServers[i].ccMail));
		pb += snprintf(pb, pe-pb, "Subject=\"%s\"\r\n", copy_with_escape(escapeBuf, pSmtpServerList->smtpServers[i].subject));
		pb += snprintf(pb, pe-pb, "/>\r\n");

	}

	pb += snprintf(pb, pe-pb, "</SMTPList>\r\n");

	return buf;
}


char *anj_config_server_conver_xml(ServerConfig* pServerCfg)
{
	int initSize = 100;
	char *tmp = NULL;
	int  incrSize = 0;

	char *buf = (char*)anj_mw_malloc(initSize);
	memset(buf, '\0', initSize);
	char *pe = buf + initSize - 1;
	char *pb = buf;
	int curSize = initSize;
	int curPos = 0;
	
	pb += snprintf(pb, pe-pb, "<ServerConfig>\r\n");

	FtpServerList  ftpServerList;
	memcpy(&ftpServerList,  pServerCfg->ftpServers,  sizeof(FtpServerList));

	curPos =  pb - buf;
	tmp =  anj_config_server_ftp_conver_xml(&ftpServerList, 1);
	incrSize = strlen(tmp);
	buf= (char*)anj_mw_realloc(buf, curSize + incrSize);
	curSize = curSize + incrSize;
	memcpy(buf + curPos, tmp, strlen(tmp));	
	curPos = curPos + strlen(tmp);
	pb = buf + curPos;	
	pe = buf + curSize - 1;
	anj_mw_free(tmp);


	curPos =  pb - buf;
	tmp = anj_config_server_smtp_conver_xml(&(pServerCfg->smtpServers), 1);
	incrSize = strlen(tmp);
	buf= (char*)anj_mw_realloc(buf, curSize + incrSize);
	curSize = curSize + incrSize;
	memcpy(buf + curPos, tmp, strlen(tmp));	
	curPos = curPos + strlen(tmp);
	pb = buf + curPos;	
	pe = buf + curSize - 1;
	anj_mw_free(tmp);


	pb += snprintf(pb, pe-pb, "</ServerConfig>\r\n");
	return buf;

}

int anj_config_server_save(ServerConfig *pServerCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_server_conver_xml(pServerCfg);
    iRet = anj_config_save_node(pDataXml, "<ServerConfig>", "</ServerConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_server_set(ServerConfig *pServerCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    ServerConfig *pOldServerCfg = (ServerConfig *)getServerConfig();
    if (memcmp(pOldServerCfg, pServerCfg, sizeof(ServerConfig)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pOldServerCfg, pServerCfg, sizeof(ServerConfig));
        anj_config_server_save(pOldServerCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}


int anj_config_server_ftp_set(FtpServerList *pstFtpList)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    ServerConfig *pOldServerCfg = (ServerConfig *)getServerConfig();

    if (memcmp(pOldServerCfg->ftpServers, pstFtpList->ftpServers, sizeof(FtpServer) * FTP_SERVER_COUNT))
    {
        __WARN("Changed!!!\n");
        memcpy(pOldServerCfg->ftpServers, pstFtpList->ftpServers, sizeof(FtpServer) * FTP_SERVER_COUNT);
        anj_config_server_save(pOldServerCfg);
    }

    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_server_smtp_list_set(SmtpServerList *pSmtpListCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    ServerConfig *pServerCfg = (ServerConfig *)getServerConfig();
    SmtpServerList *pOldSmtpListCfg = &pServerCfg->smtpServers;

    if (memcmp(pOldSmtpListCfg, pSmtpListCfg, sizeof(ServerConfig)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pOldSmtpListCfg, pSmtpListCfg, sizeof(ServerConfig));

        anj_config_server_save(pServerCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}


int anj_config_server_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf)
{
    if (NULL == pServerCfg || NULL == xmlBuf)
    {
        __ERR("error:param is null\n");
        return -1;
    }

	IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
	if(NULL == pDocNode)
	{
		__ERR("xml error\r\n");
		return -1;
	}

	IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "ServerConfig");
	if(pNodelist != NULL)
	{	
		anj_config_server_get(pNodelist->nodeItem, pServerCfg);
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

FtpServer *anj_config_server_ftp_get_by_id(ServerConfig *pServerCfg, int id)
{
    int i = 0;
    FtpServer *pstFtpServer = NULL;

    for(i = 0; i < FTP_SERVER_COUNT; i++)
    {
        if(pServerCfg->ftpServers[i].index == id)
        {
            pstFtpServer = &(pServerCfg->ftpServers[i]);
            break;
        }
    }

	return pstFtpServer;
}

int anj_config_server_ftp_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf)
{
    if (NULL == pServerCfg || NULL == xmlBuf)
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

	memset(pServerCfg->ftpServers, 0, sizeof(FtpServer) * FTP_SERVER_COUNT);
	
	IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "FTPList");
	if(pNodelist != NULL)
	{	
		IXML_Node *childNode = pNodelist->nodeItem->firstChild;
		
		int i = 0;
		while(childNode)
		{			
			if(i >= FTP_SERVER_COUNT)
			{
				break;
			}
			
			anj_config_server_ftp_get(childNode, &(pServerCfg->ftpServers[i]));
			i++;
			childNode = childNode->nextSibling;
		}
		
		ixmlNodeList_free(pNodelist);
		ixmlDocument_free(pDocNode);
		
		if(i != FTP_SERVER_COUNT)
		{
			__ERR("ftp server count=%d, old should be %d\n", i, FTP_SERVER_COUNT);
		}			
	}
	else
	{
		ixmlDocument_free(pDocNode);			
		return -1;
	}

	return 0;
}

int anj_config_server_smtp_get_by_xml(ServerConfig *pServerCfg, char *xmlBuf)
{
    if (NULL == pServerCfg || NULL == xmlBuf)
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

	IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "SMTPList");
	if(pNodelist != NULL)
	{	
		anj_config_server_smtp_list_get(pNodelist->nodeItem, &(pServerCfg->smtpServers));
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

int anj_config_server_load(ServerConfig *pServerCfg)
{
    return anj_config_load("ServerConfig", pServerCfg, 0);
}

