#include <string.h>

#include "ixml.h"
#include "anj_mw_comm.h"
#include "anj_comm.h"
#include "anj_service.h"

int anj_service_cmd_convert_xml(char *cmdbuf, char *root_name)
{
    int iRet = 0;
    ANJ_CHK((cmdbuf != NULL) && (root_name != NULL), -1, "input Invalid\n");

    if (strstr(cmdbuf, XML_ROOT_NAME1))
    {
        strcpy(root_name, XML_ROOT_NAME1);
    }
    else if (strstr(cmdbuf, XML_ROOT_NAME2))
    {
        strcpy(root_name, XML_ROOT_NAME2);
    }
    else if (strstr(cmdbuf, XML_ROOT_NAME3))
    {
        strcpy(root_name, XML_ROOT_NAME3);
    }
    else
    {
        __ERR("No valid XML root found in buffer: %s\n", cmdbuf);
        iRet = -1;
        goto endFunc;
    }
endFunc:
    return iRet;
}

int anj_service_cmd_parse_xml(IXML_Document *pDoc, char *MsgRoot, char *MsgType, char *MsgCode, char *MsgFlag, int *channel)
{
    int iRet = 0;
    ANJ_CHK((pDoc != NULL) && (MsgRoot != NULL) && (MsgType != NULL) && (MsgCode != NULL) && (MsgFlag != NULL) && (channel != NULL),
            -1, "input Invalid");

    IXML_NodeList *pRootNode = ixmlDocument_getElementsByTagName(pDoc, MsgRoot);
    ANJ_CHK((pRootNode != NULL), -1, "bad msg header");
    ixmlNodeList_free(pRootNode);

    const char *tagName = "MESSAGE_HEADER";
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, tagName);
    ANJ_CHK((pNodelist != NULL), -1, "bad msg header");

    IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
    while (tmp != NULL)
    {
        if (strcmp(tmp->nodeName, "Msg_type") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgType, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_code") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgCode, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_flag") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                strcpy(MsgFlag, tmp->nodeValue);
            }
        }
        else if (strcmp(tmp->nodeName, "Msg_channel") == 0)
        {
            if (tmp->nodeValue != NULL)
            {
                *channel = atoi(tmp->nodeValue);
                if (*channel > ANJ_CAMERA_MAX_NUMS || *channel < 0)
                    *channel = 0;
            }
        }

        tmp = tmp->nextSibling;
    }

    ixmlNodeList_free(pNodelist);
endFunc:
    return iRet;
}
