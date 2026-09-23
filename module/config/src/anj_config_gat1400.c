#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_config.h"


int anj_config_gat1400_get(IXML_Node *pNode, GAT1400Config *pGat1400Cfg)
{
    memset(pGat1400Cfg, 0, sizeof(GAT1400Config));

    IXML_Node* tmpChild = pNode->firstChild;
    IXML_Node* tmpAttr = NULL;
    char escapeBuf[1024] = {0};

    //default value if not found
    pGat1400Cfg->enable=0;

    while(tmpChild != NULL)
    {
        if(!strcmp(tmpChild->nodeName, "GAT1400"))
        {
            __ERR("found GAT1400 tag\n");           
            tmpAttr = tmpChild->firstAttr;
            while(tmpAttr != NULL)
            {
                if(!strcmp(tmpAttr->nodeName, "Enable"))
                {
                    pGat1400Cfg->enable = Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "Https"))
                {
                    pGat1400Cfg->https = Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "StreamType"))
                {
                    pGat1400Cfg->stream_type = Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "KeepAliveTime"))
                {
                    pGat1400Cfg->keepalive_time = Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "KeepAliveCount"))
                {
                    pGat1400Cfg->keepalive_count = Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "AlarmTime"))
                {
                    pGat1400Cfg->alarm_time= Str2Num(tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "UserName"))
                {
                    StrCpy(pGat1400Cfg->username, GAT1400_NAME_MAX_LEN, copy_with_escape(escapeBuf,tmpAttr->nodeValue));
                }
                else if(!strcmp(tmpAttr->nodeName, "Password"))
                {
                    StrCpy(pGat1400Cfg->password, GAT1400_PWD_MAX_LEN, copy_with_escape(escapeBuf,tmpAttr->nodeValue));
                }
                else if(!strcmp(tmpAttr->nodeName, "DeviceID"))
                {
                    StrCpy(pGat1400Cfg->device_id, GAT1400_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "ChannelID"))
                {
                    StrCpy(pGat1400Cfg->channel_id, GAT1400_ID_MAX_LEN,tmpAttr->nodeValue);
                }
                else if(!strcmp(tmpAttr->nodeName, "ServerAddr"))
                {
                    StrCpy(pGat1400Cfg->server_addr, GAT1400_SERVER_NAME_MAX_LEN, copy_with_escape(escapeBuf,tmpAttr->nodeValue));
                }
                else if(!strcmp(tmpAttr->nodeName, "ServerPort"))
                {
                    pGat1400Cfg->server_port = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;             
            }
            
            break;
        }

        tmpChild = tmpChild->nextSibling;
    }

    __INFO("gat 1400 enable:%d!\n", pGat1400Cfg->enable);

    return 0;
}

char *anj_config_gat1400_conver_xml(GAT1400Config* pGat1400Cfg)
{
    int maxSize = 2000;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char*)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize -1;

    pb += snprintf(pb, pe-pb, "<GAT1400Config>\r\n");

    pb += snprintf(pb, pe-pb, "<GAT1400\r\n");
    pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n",    pGat1400Cfg->enable);
    pb += snprintf(pb, pe-pb, "Https=\"%d\"\r\n",     pGat1400Cfg->https);
    pb += snprintf(pb, pe-pb, "StreamType=\"%d\"\r\n",    pGat1400Cfg->stream_type);
    pb += snprintf(pb, pe-pb, "KeepAliveTime=\"%d\"\r\n",     pGat1400Cfg->keepalive_time);
    pb += snprintf(pb, pe-pb, "KeepAliveCount=\"%d\"\r\n",    pGat1400Cfg->keepalive_count);
    pb += snprintf(pb, pe-pb, "AlarmTime=\"%d\"\r\n",     pGat1400Cfg->alarm_time);
    pb += snprintf(pb, pe-pb, "UserName=\"%s\"\r\n",      pGat1400Cfg->username);
    pb += snprintf(pb, pe-pb, "Password=\"%s\"\r\n",      pGat1400Cfg->password);
    pb += snprintf(pb, pe-pb, "DeviceID=\"%s\"\r\n",      pGat1400Cfg->device_id);
    pb += snprintf(pb, pe-pb, "ChannelID=\"%s\"\r\n",     pGat1400Cfg->channel_id);
    pb += snprintf(pb, pe-pb, "ServerAddr=\"%s\"\r\n",    pGat1400Cfg->server_addr);
    pb += snprintf(pb, pe-pb, "ServerPort=\"%d\"\r\n",    pGat1400Cfg->server_port);
    
    pb += snprintf(pb, pe-pb, "/>\r\n"); 
    pb += snprintf(pb, pe-pb, "</GAT1400Config>\r\n");

    return buf;
}



int anj_config_gat1400_save(GAT1400Config *pGat1400Cfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_gat1400_conver_xml(pGat1400Cfg);
    iRet = anj_config_save_node(pDataXml, "<GAT1400Config>", "</GAT1400Config>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_gat1400_set(GAT1400Config *pGat1400Cfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    GAT1400Config *pOldGat1400cfg = (GAT1400Config *)getGat1400Config();
    if (memcmp(pOldGat1400cfg, pGat1400Cfg, sizeof(GAT1400Config)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pOldGat1400cfg, pGat1400Cfg, sizeof(GAT1400Config));
        anj_config_gat1400_save(pOldGat1400cfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}


int anj_config_gat1400_get_by_xml(GAT1400Config *pGat1400Cfg, char *xmlBuf)
{
    if (NULL == pGat1400Cfg || NULL == xmlBuf)
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

    IXML_NodeList* pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "GAT1400Config");
    if(pNodelist != NULL)
    {   
        anj_config_gat1400_get(pNodelist->nodeItem, pGat1400Cfg);
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


int anj_config_gat1400_load(GAT1400Config *pGat1400Cfg)
{
    return anj_config_load("GAT1400Config", pGat1400Cfg, 0);
}


