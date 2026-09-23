#include "anj_config_oem.h"
#include "anj_config.h"
#include "anj_comm.h"
#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_sysmng.h"

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static IXML_Node *anj_config_oem_xml_get(IXML_NodeList *my_pNodelist, const char *my_node_name) // 获取节点点数
{
    IXML_Node *my_pNode = my_pNodelist->nodeItem->firstChild;
    if (my_pNode == NULL)
    {
        return NULL;
    }
    while (my_pNode != NULL)
    {
        if (strcmp(my_pNode->nodeName, my_node_name) == 0)
            break;
        my_pNode = my_pNode->nextSibling;
    }
    return my_pNode;
}

static int anj_config_oem_xml_set(IXML_Node *my_pNode, SECOND_DEFAULTCONFIG_DATA *pConfig)
{
    __ERR("anj_config_oem_xml_set\n");
    if (strcmp(my_pNode->nodeName, "Configuration") == 0)
    {
        IXML_Node *pNode = my_pNode->firstAttr;
        while (pNode != NULL)
        {
            if (pNode->nodeValue)
            {
                __ERR("pNode->nodeName:%s\n", pNode->nodeName);
                __ERR("pNode->nodeValue:%s\n", pNode->nodeValue);
                if (strcmp(pNode->nodeName, "Name") == 0)
                    strcpy(pConfig->device_name, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "GB_publisher") == 0)
                    strcpy(pConfig->gb_publisher, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Title_XY") == 0)
                    strcpy(pConfig->title_xy, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Time_XY") == 0)
                    strcpy(pConfig->time_xy, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Lan") == 0)
                    strcpy(pConfig->lan, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Password") == 0)
                    strcpy(pConfig->password, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "IE_Lan") == 0)
                    strcpy(pConfig->ie_lan, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Title") == 0)
                    strcpy(pConfig->title, pNode->nodeValue);
            }
            pNode = pNode->nextSibling;
        }
    }
    else if (strcmp(my_pNode->nodeName, "Conctrol") == 0)
    {
        IXML_Node *pNode = my_pNode->firstAttr;
        while (pNode != NULL)
        {
            if (pNode->nodeValue)
            {
                __ERR("pNode->nodeName:%s\n", pNode->nodeName);
                __ERR("pNode->nodeValue:%s\n", pNode->nodeValue);
                if (strcmp(pNode->nodeName, "Sen_c_li") == 0)
                    pConfig->sen_c_li = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Sen_o_li") == 0)
                    pConfig->sen_o_li = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Audio_out") == 0)
                    pConfig->audio_out = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Audio_in") == 0)
                    pConfig->audio_in = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Li_pw") == 0)
                    pConfig->li_pw = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Stream_Brightness") == 0)
                    pConfig->brightness = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Stream_Saturation") == 0)
                    pConfig->saturation = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Stream_Sharpness") == 0)
                    pConfig->sharpness = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Stream_Contrast") == 0)
                    pConfig->contrast = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "LED") == 0)
                    pConfig->led = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_0") == 0)
                    strcpy(pConfig->resolution_value_0, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_1") == 0)
                    strcpy(pConfig->resolution_value_1, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_2") == 0)
                    strcpy(pConfig->resolution_value_2, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_0_fake") == 0)
                    strcpy(pConfig->resolution_value_0_fake, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_1_fake") == 0)
                    strcpy(pConfig->resolution_value_1_fake, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Resolution_value_2_fake") == 0)
                    strcpy(pConfig->resolution_value_2_fake, pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Encoder_0") == 0)
                    pConfig->encoder_0 = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Encoder_1") == 0)
                    pConfig->encoder_1 = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Encoder_2") == 0)
                    pConfig->encoder_2 = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Privacy_PTZ_Direction_Value") == 0)
                    pConfig->privacy_ptz_direction_value = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Tvsystem") == 0)
                    pConfig->tvsystem = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "KC_mode") == 0)
                    pConfig->kc_mode = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "PTZ_speed") == 0)
                    pConfig->ptz_speed = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Low_Li_pw") == 0)
                    pConfig->low_li_pw = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "OptimumDistance") == 0)
                    pConfig->optimumdistance = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "alarm_audio_switch") == 0)
                    pConfig->alarm_audio_switch = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "aov_workmode") == 0)
                    pConfig->aov_workmode = atoi(pNode->nodeValue);
            }
            pNode = pNode->nextSibling;
        }
    }
    else if (strcmp(my_pNode->nodeName, "Capability") == 0)
    {
        IXML_Node *pNode = my_pNode->firstAttr;
        while (pNode != NULL)
        {
            if (pNode->nodeValue)
            {
                __ERR("pNode->nodeName:%s\n", pNode->nodeName);
                __ERR("pNode->nodeValue:%s\n", pNode->nodeValue);
                if (strcmp(pNode->nodeName, "PTZ_Yuntai") == 0)
                    pConfig->ptz_yuntai = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "PTZ_Zoom") == 0)
                    pConfig->ptz_zoom = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "PTZ_AF") == 0)
                    pConfig->ptz_af = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "PTZ_Track") == 0)
                    pConfig->ptz_track = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "PTZ_Cruise") == 0)
                    pConfig->ptz_cruise = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Cover") == 0)
                    pConfig->cover = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Call") == 0)
                    pConfig->call = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "Low_pw") == 0)
                    pConfig->low_pw = atoi(pNode->nodeValue);
                else if (strcmp(pNode->nodeName, "LED_type") == 0)
                    pConfig->led_type = atoi(pNode->nodeValue);
            }
            pNode = pNode->nextSibling;
        }
    }
    else
        __ERR("anj_config_oem_xml_set cant find this pin!!\n");
    return 0;
}

static int anj_config_oem_load(SECOND_DEFAULTCONFIG_DATA *myconfig)
{
    int iRet = 0;
    char *pCfgXml = NULL;

    ANJ_CHK((myconfig != NULL), -1, "input Invalid");
    char filePath[64] = {0};
    snprintf(filePath, sizeof(filePath), "%s", OEM_SECOND_CONFIG_PATH);

    pCfgXml = anj_mw_read_file_buffer(filePath);
    ANJ_CHK(pCfgXml != NULL, -1, "read failed");

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    IXML_NodeList *pNodelist = NULL;
    pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DefaultConfig");
    ANJ_CHK(pNodelist != NULL, -1, "DefaultConfig Parse error");

    IXML_Node *pNode_Configuration = NULL;
    IXML_Node *pNode_Conctrol = NULL;
    IXML_Node *pNode_Capability = NULL;

    pNode_Configuration = anj_config_oem_xml_get(pNodelist, "Configuration");
    if (pNode_Configuration != NULL)
        anj_config_oem_xml_set(pNode_Configuration, myconfig);
    else
        __ERR("anj_config_oem_xml_get err1!\n");

    pNode_Conctrol = anj_config_oem_xml_get(pNodelist, "Conctrol");
    if (pNode_Conctrol != NULL)
        anj_config_oem_xml_set(pNode_Conctrol, myconfig);
    else
        __ERR("anj_config_oem_xml_get err2!\n");

    pNode_Capability = anj_config_oem_xml_get(pNodelist, "Capability");
    if (pNode_Capability != NULL)
        anj_config_oem_xml_set(pNode_Capability, myconfig);
    else
        __ERR("anj_config_oem_xml_get err3!\n");
    ixmlNodeList_free(pNodelist);
    ixmlDocument_free(pDocNode);
endFunc:
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    return iRet;
}

/*

<?xml version="1.0" encoding="gb2312" ?>
<OEM_CONFIG>
    <SYSTEM DEVICETYPE="设备型号" OEMVERSION="软件版本, 如1.0.1" OEMBUILDTIME="打包日期,如20170101" SN="OEM序列号" HW="硬件版本, 如1" ETHMAC="以太网卡MAC"  />
</OEM_CONFIG>
*/
static void anj_config_oem_attr_get(const char *xml, const char *key, char *dst, int dstLen)
{
    char pat[40] = {0};
    const char *p = NULL;
    const char *e = NULL;
    int n = 0;

    if (xml == NULL || key == NULL || dst == NULL || dstLen <= 0)
    {
        return;
    }

    snprintf(pat, sizeof(pat), "%s=\"", key);
    p = strstr(xml, pat);
    if (p == NULL)
    {
        return;
    }

    p += strlen(pat);
    e = strchr(p, '"');
    if (e == NULL)
    {
        return;
    }

    n = (int)(e - p);
    if (n >= dstLen)
    {
        n = dstLen - 1;
    }
    if (n > 0)
    {
        memcpy(dst, p, n);
    }
    dst[n] = 0;
}

static int anj_config_oem_parase(char *pCfgXml, AjOemStruct *pInfo)
{
    int iRet = 0;
    ANJ_CHK(((pCfgXml != NULL) && (pInfo != NULL)), -1, "input Invalid");

    if (strstr(pCfgXml, "OEM_CONFIG") == NULL || strstr(pCfgXml, "SYSTEM") == NULL)
    {
        __ERR("xmlDocument_getElementsByTagName(OEM_CONFIG) return NULL!\n");
        iRet = -1;
        goto endFunc;
    }

    anj_config_oem_attr_get(pCfgXml, "DEVICETYPE", pInfo->szDeviceType, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "OEMVERSION", pInfo->szVersion, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "OEMBUILDTIME", pInfo->szBuildtime, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "SN", pInfo->szOemSN, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "HW", pInfo->szOemHWVersion, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "ETHMAC", pInfo->szOemEthMac, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "WIFIMAC", pInfo->szOemWifiMac, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "MBL", pInfo->szOemMBL, AJ_OEM_STR_LEN);
    anj_config_oem_attr_get(pCfgXml, "LANGUAGE", pInfo->szOemLanguage, AJ_OEM_STR_LEN);

endFunc:
    return iRet;
}

static int anj_config_oem_xml_load(const char *path, AjOemStruct *pInfo)
{
    char xmlBuf[1024] = {0};

    if (anj_mw_read_file_limit_len(path, xmlBuf, (int)sizeof(xmlBuf) - 1) <= 0)
    {
        return -1;
    }
    return anj_config_oem_parase(xmlBuf, pInfo);
}

static int anj_config_oem_cust_get(AjOemStruct *pInfo)
{
    int default_2_name = 0;

    if (anj_config_oem_second_gate_b())
    {
        SECOND_DEFAULTCONFIG_DATA myconfig = {0};
        anj_config_oem_second_load(&myconfig);
        if (strlen(myconfig.device_name) > 0)
        {
            default_2_name = 1;
            strcpy(pInfo->szDeviceType, myconfig.device_name);
        }
    }

    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_SPECIFIC_FILE_NAME);

    if (anj_mw_file_exists(szOemXMLFileName))
    {
        return anj_config_oem_xml_load(szOemXMLFileName, pInfo);
    }
    if (default_2_name == 0)
    {
        memset(szOemXMLFileName, 0, sizeof(szOemXMLFileName));
        snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", AJ_APP_PATH, AJ_XML_SPECIFIC_FILE_NAME);
        if (anj_mw_file_exists(szOemXMLFileName))
        {
            return anj_config_oem_xml_load(szOemXMLFileName, pInfo);
        }
    }

    if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH) == 0)
    {
        __ERR("can not read oem specific config file\r\n");
        strncpy(pInfo->szDeviceType, ANJ_DEVICE_TYPE, AJ_OEM_STR_LEN - 1);
    }

    return 0;
}

static int anj_config_oem_second_blocked(void)
{
    DevInfo *pstDevInfo = getDevInfo();
    if (pstDevInfo != NULL && pstDevInfo->bFactoryMode)
    {
        return 1;
    }
    if (anj_mw_file_exists(DATA_BLOCK_MOUNT_PATH "/" AJ_CUST_PATH_NAME "/" AJ_XML_SPECIFIC_FILE_NAME))
    {
        return 1;
    }
    if (anj_mw_file_exists(OEM_SECOND_GENERIC_DEFAULT_XML))
    {
        return 1;
    }
    return 0;
}

int anj_config_oem_second_gate_a(void)
{
    if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH) == 0)
    {
        return 0;
    }
    if (anj_mw_file_exists(OEM_SECOND_SAVE_FLAG_PATH) == 0 &&
        anj_mw_file_exists(OEM_SECOND_RESTORE_FLAG_PATH) == 0)
    {
        return 0;
    }
    if (anj_config_oem_second_blocked())
    {
        return 0;
    }
    return 1;
}

int anj_config_oem_second_gate_b(void)
{
    if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH) == 0)
    {
        return 0;
    }
    if (anj_mw_file_exists(OEM_SECOND_INIT_FLAG_PATH))
    {
        return 1;
    }
    if (anj_config_oem_second_blocked() == 0)
    {
        return 1;
    }
    return 0;
}

int anj_config_oem_second_load(SECOND_DEFAULTCONFIG_DATA *myconfig)
{
    if (anj_mw_file_exists(OEM_SECOND_CONFIG_PATH) == 0)
    {
        return -1;
    }
    else
    {
        // control
        myconfig->audio_out = 0xffff;
        myconfig->audio_in = 0xffff;
        myconfig->sen_o_li = 0xffff;
        myconfig->sen_c_li = 0xffff;
        myconfig->li_pw = 0xffff;
        myconfig->led = 0xffff;
        myconfig->brightness = 0xffff;
        myconfig->saturation = 0xffff;
        myconfig->sharpness = 0xffff;
        myconfig->contrast = 0xffff;
        myconfig->encoder_0 = 0xffff;
        myconfig->encoder_1 = 0xffff;
        myconfig->encoder_2 = 0xffff;
        myconfig->privacy_ptz_direction_value = 0xffff;
        myconfig->tvsystem = 0xffff;
        myconfig->kc_mode = 0xffff;
        myconfig->ptz_speed = 0xffff;
        myconfig->low_li_pw = 0xffff;
        myconfig->optimumdistance = 0xffff;
        myconfig->alarm_audio_switch = 0xffff;
        myconfig->aov_workmode = 0xffff;
        // Capability
        myconfig->ptz_yuntai = 0xffff;
        myconfig->ptz_zoom = 0xffff;
        myconfig->ptz_af = 0xffff;
        myconfig->ptz_track = 0xffff;
        myconfig->ptz_cruise = 0xffff;
        myconfig->cover = 0xffff;
        myconfig->call = 0xffff;
        myconfig->low_pw = 0xffff;
        myconfig->led_type = 0xffff;
    }

    anj_config_oem_load(myconfig);
    return 0;
}

// 读取AJ出厂的OEM INFO
int anj_config_oem_factory_get(AjOemStruct *pInfo)
{
    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", AJ_APP_PATH, AJ_XML_SPECIFIC_FILE_NAME);

    if (anj_mw_file_exists(szOemXMLFileName))
    {
        return anj_config_oem_xml_load(szOemXMLFileName, pInfo);
    }

    __INFO("can not read oem specific config file\r\n");
    strncpy(pInfo->szDeviceType, ANJ_DEVICE_TYPE, AJ_OEM_STR_LEN - 1);

    return 0;
}

int anj_config_oem_get(AjOemStruct *pInfo)
{
    int iRet = 0;

    if (0 == anj_config_oem_cust_get(pInfo))
    {
        AjOemStruct factoryInfo = {0};

        if (0 == anj_config_oem_factory_get(&factoryInfo))
        {
            if (strlen(pInfo->szDeviceType) == 0 && strlen(factoryInfo.szDeviceType) > 0)
                strcpy(pInfo->szDeviceType, factoryInfo.szDeviceType);
            if (strlen(pInfo->szVersion) == 0 && strlen(factoryInfo.szVersion) > 0)
                strcpy(pInfo->szVersion, factoryInfo.szVersion);
            if (strlen(pInfo->szBuildtime) == 0 && strlen(factoryInfo.szBuildtime) > 0)
                strcpy(pInfo->szBuildtime, factoryInfo.szBuildtime);
            if (strlen(pInfo->szOemSN) == 0 && strlen(factoryInfo.szOemSN) > 0)
                strcpy(pInfo->szOemSN, factoryInfo.szOemSN);
            if (strlen(pInfo->szOemHWVersion) == 0 && strlen(factoryInfo.szOemHWVersion) > 0)
                strcpy(pInfo->szOemHWVersion, factoryInfo.szOemHWVersion);
            if (strlen(pInfo->szOemEthMac) == 0 && strlen(factoryInfo.szOemEthMac) > 0)
                strcpy(pInfo->szOemEthMac, factoryInfo.szOemEthMac);
            if (strlen(pInfo->szOemWifiMac) == 0 && strlen(factoryInfo.szOemWifiMac) > 0)
                strcpy(pInfo->szOemWifiMac, factoryInfo.szOemWifiMac);
            if (strlen(pInfo->szOemMBL) == 0 && strlen(factoryInfo.szOemMBL) > 0)
                strcpy(pInfo->szOemMBL, factoryInfo.szOemMBL);
            if (strlen(pInfo->szOemLanguage) == 0 && strlen(factoryInfo.szOemLanguage) > 0)
                strcpy(pInfo->szOemLanguage, factoryInfo.szOemLanguage);
        }

        iRet = 0;
    }
    else
    {
        iRet = anj_config_oem_factory_get(pInfo);
    }

    return iRet;
}

int anj_config_oem_save(AjOemStruct *pInfo)
{
    int iRet = 0;
    FILE *fp = NULL;
    char *pxmlBuf = NULL;
    ANJ_CHK((pInfo != NULL), -1, "input Invalid");
    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);

    if (anj_mw_file_exists(szOemXMLFileName) == 0)
    {
        ANJ_CHK_FUNC(mkdir(szOemXMLFileName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH), 0, "open oem file path failed");
    }

    sprintf(szOemXMLFileName, "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_SPECIFIC_FILE_NAME);
    fp = anj_mw_fopen(szOemXMLFileName, "w+");
    ANJ_CHK((fp != NULL), -1, "open oem file failed!");

    pxmlBuf = anj_mw_malloc(4096);
    ANJ_CHK((pxmlBuf != NULL), -1, "malloc failed!");

    snprintf(pxmlBuf, 4096, "<?xml version=\"1.0\" encoding=\"gb2312\" ?>\r\n"
                            "<OEM_CONFIG>\r\n"
                            "<SYSTEM DEVICETYPE=\"%s\" OEMVERSION=\"%s\" OEMBUILDTIME=\"%s\" SN=\"%s\" HW=\"%s\""
                            " ETHMAC=\"%s\" WIFIMAC=\"%s\" MBL=\"%s\" LANGUAGE=\"%s\"\r\n"
                            " />\r\n"
                            "</OEM_CONFIG>\r\n",
             pInfo->szDeviceType, pInfo->szVersion, pInfo->szBuildtime, pInfo->szOemSN, pInfo->szOemHWVersion,
             pInfo->szOemEthMac, pInfo->szOemWifiMac, pInfo->szOemMBL, pInfo->szOemLanguage);

    iRet = anj_mw_fwrite(fp, pxmlBuf, strlen(pxmlBuf));
    if (iRet != strlen(pxmlBuf))
    {
        __ERR("anj_mw_fwrite fail\n");
        iRet = -1;
        goto endFunc;
    }

endFunc:
    if (pxmlBuf)
    {
        anj_mw_free(pxmlBuf);
    }
    if (fp)
    {
        anj_mw_fclose(fp);
    }
    return iRet;
}

int anj_config_web_test_get(TestWebSiteStruct *pInfo)
{
    int iRet = 0;
    char *pCfgXml = NULL;
    ANJ_CHK((pInfo != NULL), -1, "input Invalid");

    memset(pInfo, 0, sizeof(TestWebSiteStruct));

    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_TEST_WEBSITE_FILE_NAME);

    pCfgXml = anj_mw_read_file_buffer(szOemXMLFileName);
    // 如果没有上传最新的话，那么就从opt/ch目录选取
    if (pCfgXml == NULL)
    {
        snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", AJ_APP_PATH, AJ_XML_TEST_WEBSITE_FILE_NAME);
        pCfgXml = anj_mw_read_file_buffer(szOemXMLFileName);
        ANJ_CHK((pCfgXml != NULL), -1, "can not read oem specific config file");
    }

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    anj_mw_free(pCfgXml);
    pCfgXml = NULL;

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "WEBSITE_CONFIG");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpChild = pNode->firstChild;
        IXML_Node *tmpAttr = NULL;

        while (tmpChild != NULL)
        {
            if (strcmp(tmpChild->nodeName, "WEBSITE") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "website_name1") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->website1, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "website_name2") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->website2, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "website_name3") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->website3, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "website_name4") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->website4, tmpAttr->nodeValue, AJ_XML_TEST_WEBSITE4_LEN - 1);
                        }
                    }

                    tmpAttr = tmpAttr->nextSibling;
                }
            }

            tmpChild = tmpChild->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(WEBSITE_CONFIG) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
endFunc:
    return iRet;
}

int anj_config_oem_onvif_get(OnvifOemStruct *pInfo)
{
    int iRet = 0;
    char *pCfgXml = NULL;
    ANJ_CHK((pInfo != NULL), -1, "input Invalid");

    memset(pInfo, 0, sizeof(OnvifOemStruct));
    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_ONVIF_OEM_FILE_NAME);

    pCfgXml = anj_mw_read_file_buffer(szOemXMLFileName);
    if (pCfgXml == NULL)
    {
        memset(szOemXMLFileName, 0, sizeof(szOemXMLFileName));
        snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", AJ_APP_PATH, AJ_XML_ONVIF_OEM_FILE_NAME);
        pCfgXml = anj_mw_read_file_buffer(szOemXMLFileName);
        ANJ_CHK((pCfgXml != NULL), -1, "can not read oem specific config file");
    }

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    anj_mw_free(pCfgXml);
    pCfgXml = NULL;

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "ONVIF_CONFIG");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpChild = pNode->firstChild;
        IXML_Node *tmpAttr = NULL;

        while (tmpChild != NULL)
        {
            if (strcmp(tmpChild->nodeName, "OEM") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "name") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->name, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "location") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->location, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "city") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->city, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "manufacturer") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->manufacturer, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "model") == 0)
                    {
                        if (NULL != tmpAttr->nodeValue)
                        {
                            strncpy(pInfo->model, tmpAttr->nodeValue, AJ_OEM_STR_LEN - 1);
                        }
                    }

                    tmpAttr = tmpAttr->nextSibling;
                }
            }

            tmpChild = tmpChild->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(ONVIF_CONFIG) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
endFunc:
    return iRet;
}

char *anj_config_oem_test_website_conver_msg_xml(TestWebSiteStruct *pCfg)
{
    int maxSize = 512;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    char escapeBuf[512];

    buf = (char*)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize -1;

    pb += snprintf(pb, pe-pb, "<WEBSITE\r\n");
    pb += snprintf(pb, pe-pb, "website_name1=\"%s\"\r\n", copy_with_escape(escapeBuf,pCfg->website1));
    pb += snprintf(pb, pe-pb, "website_name2=\"%s\"\r\n", copy_with_escape(escapeBuf,pCfg->website2)); 
    pb += snprintf(pb, pe-pb, "website_name3=\"%s\"\r\n", copy_with_escape(escapeBuf,pCfg->website3));
    pb += snprintf(pb, pe-pb, "website_name4=\"%s\"\r\n", copy_with_escape(escapeBuf,pCfg->website4));	
    pb += snprintf(pb, pe-pb, "/>\r\n");

    return buf;
}

