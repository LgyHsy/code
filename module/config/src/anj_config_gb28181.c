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

#include "eventhub.h"

int anj_config_gb28181_get(IXML_Node *pNode, GB28181Config *pGb28181Cfg)
{
    if (NULL == pNode || NULL == pGb28181Cfg)
    {
        return -1;
    }

    IXML_Node *tmpAttr = NULL;
    IXML_Node *pChildNode = pNode->firstChild;

    char escapeBuf[1024] = {0};
    pGb28181Cfg->enable = 0;

    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "GB28181"))
        {
            __INFO("found GB28181 tag\n");

            tmpAttr = pChildNode->firstAttr;

            while (tmpAttr != NULL)
            {
                if (!strcmp(tmpAttr->nodeName, "Enable"))
                {
                    pGb28181Cfg->enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "tcp"))
                {
                    pGb28181Cfg->tcp = (short)Str2Num(tmpAttr->nodeValue);
                    pGb28181Cfg->protocol = pGb28181Cfg->tcp;
                }
                else if (!strcmp(tmpAttr->nodeName, "nstreams"))
                {
                    pGb28181Cfg->nstreams = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "nChannelNum"))
                {
                    pGb28181Cfg->nChannelNum = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "hcId"))
                {
                    StrCpy(pGb28181Cfg->hcId, GB28181_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "hcIp"))
                {
                    StrCpy(pGb28181Cfg->hcIp, GB28181_IP_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "hcName"))
                {
                    StrCpy(pGb28181Cfg->hcName, GB28181_NAME_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "hcPwd"))
                {
                    StrCpy(pGb28181Cfg->hcPwd, GB28181_PWD_MAX_LEN, copy_with_escape(escapeBuf, tmpAttr->nodeValue));
                }
                else if (!strcmp(tmpAttr->nodeName, "hcPort"))
                {
                    pGb28181Cfg->hcPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lcId"))
                {
                    StrCpy(pGb28181Cfg->lcId, GB28181_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lcName"))
                {
                    StrCpy(pGb28181Cfg->lcName, GB28181_NAME_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "lcPwd"))
                {
                    StrCpy(pGb28181Cfg->lcPwd, GB28181_PWD_MAX_LEN, copy_with_escape(escapeBuf, tmpAttr->nodeValue));
                }
                else if (!strcmp(tmpAttr->nodeName, "lcPort"))
                {
                    pGb28181Cfg->lcPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "camId"))
                {
                    StrCpy(pGb28181Cfg->camId, GB28181_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarmId"))
                {
                    StrCpy(pGb28181Cfg->alarmId, GB28181_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "deviceName"))
                {
                    StrCpy(pGb28181Cfg->device_name, GB28181_ID_MAX_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "isOpen35114"))
                {
                    pGb28181Cfg->isOpen35114 = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            break;
        }

        pChildNode = pChildNode->nextSibling;
    }

    return 0;
}

char *anj_config_gb28181_conver_xml(GB28181Config *pGb28181Cfg)
{
    int initsize = 2000;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(initsize);
    memset(buf, 0, initsize);

    pb = buf;
    pe = buf + initsize - 1;

    pb += snprintf(pb, pe - pb, "<GB28181Config>\r\n");

    pb += snprintf(pb, pe - pb, "<GB28181\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pGb28181Cfg->enable);
    pb += snprintf(pb, pe - pb, "tcp=\"%d\"\r\n", pGb28181Cfg->tcp);
    pb += snprintf(pb, pe - pb, "nstreams=\"%d\"\r\n", pGb28181Cfg->nstreams);
    pb += snprintf(pb, pe - pb, "nChannelNum=\"%d\"\r\n", pGb28181Cfg->nChannelNum);
    pb += snprintf(pb, pe - pb, "hcId=\"%s\"\r\n", pGb28181Cfg->hcId);
    pb += snprintf(pb, pe - pb, "hcIp=\"%s\"\r\n", pGb28181Cfg->hcIp);
    pb += snprintf(pb, pe - pb, "hcName=\"%s\"\r\n", pGb28181Cfg->hcName);
    pb += snprintf(pb, pe - pb, "hcPwd=\"%s\"\r\n", pGb28181Cfg->hcPwd);
    pb += snprintf(pb, pe - pb, "hcPort=\"%d\"\r\n", pGb28181Cfg->hcPort);
    pb += snprintf(pb, pe - pb, "lcId=\"%s\"\r\n", pGb28181Cfg->lcId);
    pb += snprintf(pb, pe - pb, "lcName=\"%s\"\r\n", pGb28181Cfg->lcName);
    pb += snprintf(pb, pe - pb, "lcPwd=\"%s\"\r\n", pGb28181Cfg->lcPwd);
    pb += snprintf(pb, pe - pb, "lcPort=\"%d\"\r\n", pGb28181Cfg->lcPort);
    pb += snprintf(pb, pe - pb, "camId=\"%s\"\r\n", pGb28181Cfg->camId);
    pb += snprintf(pb, pe - pb, "alarmId=\"%s\"\r\n", pGb28181Cfg->alarmId);
    pb += snprintf(pb, pe - pb, "deviceName=\"%s\"\r\n", pGb28181Cfg->device_name);

    pb += snprintf(pb, pe - pb, "isOpen35114=\"%d\"\r\n", pGb28181Cfg->isOpen35114);
    pb += snprintf(pb, pe - pb, "protocol=\"%d\"\r\n", pGb28181Cfg->protocol);

    pb += snprintf(pb, pe - pb, "/>\r\n");
    pb += snprintf(pb, pe - pb, "</GB28181Config>\r\n");

    return buf;
}

int anj_config_gb28181_save(GB28181Config *pGb28181Cfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_gb28181_conver_xml(pGb28181Cfg);
    iRet = anj_config_save_node(pDataXml, "<GB28181Config>", "</GB28181Config>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_gb28181_set(GB28181Config *pGb28181Cfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    GB28181Config *pOldGb28181cfg = (GB28181Config *)getGb28181Config();
    if (memcmp(pOldGb28181cfg, pGb28181Cfg, sizeof(GB28181Config)))
    {
        __WARN("Change!!!\n");
        memcpy(pOldGb28181cfg, pGb28181Cfg, sizeof(GB28181Config));
        anj_config_gb28181_save(pOldGb28181cfg);
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_GB28181_UPDATE, &event_result, NULL);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_gb28181_get_by_xml(GB28181Config *pGb28181Cfg, char *xmlBuf)
{
    if (NULL == pGb28181Cfg || NULL == xmlBuf)
    {
        __ERR("error:param is null\n");
        return -1;
    }

    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);
    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "GB28181Config");
    if (pNodelist != NULL)
    {
        anj_config_gb28181_get(pNodelist->nodeItem, pGb28181Cfg);
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

int anj_config_gb28181_load(GB28181Config *pGb28181Cfg)
{
    return anj_config_load("GB28181Config", pGb28181Cfg, 0);
}
