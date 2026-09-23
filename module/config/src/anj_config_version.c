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


int anj_config_version_get(IXML_Node *pNode, ConfigVersion *pVersionCfg)
{
	memset(pVersionCfg, 0, sizeof(ConfigVersion));
	
	IXML_Node* tmpAttr = NULL;
	tmpAttr = pNode->firstAttr;
	while(tmpAttr != NULL)
	{
		if(!strcmp(tmpAttr->nodeName, "Version"))
		{
			StrCpy(pVersionCfg->szVersion,64,tmpAttr->nodeValue);
		}

		tmpAttr = tmpAttr->nextSibling;
	}
	return 0;
}

static char *anj_config_version_conver_xml(ConfigVersion* pVersionCfg)
{
	int maxSize = 256;
	char *pe = NULL;
	char *pb = NULL;
	char *buf = NULL;

	buf = (char*)anj_mw_malloc(maxSize);
	pb = buf;
	pe = buf + maxSize -1;

	pb += snprintf(pb, pe-pb, "\r\n<ConfigVersion Version=\"%s\"></ConfigVersion>\r\n", pVersionCfg->szVersion);

	return buf;	
}


int anj_config_version_save(ConfigVersion *pVersionCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_version_conver_xml(pVersionCfg);
    iRet = anj_config_save_node(pDataXml, "<ConfigVersion>", "</ConfigVersion>");
    anj_mw_free(pDataXml);
    return iRet;
}


int anj_config_version_load(ConfigVersion *pVersionCfg)
{
    return anj_config_load("ConfigVersion", pVersionCfg, 0);
}



