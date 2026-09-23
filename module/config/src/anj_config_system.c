#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_log.h"
#include "anj_mw_str.h"
#include "anj_mw_crypt.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_systime.h"
#include "anj_osd.h"
#include "anj_video.h"

DstConfig *ReadSummerTimeListFromXML()
{
    DstConfig *pReturn = NULL;
    DstConfig *p = NULL;

    char *pCfgXml = anj_mw_read_file_buffer("/opt/ch/tvt_tz.xml");

    if (pCfgXml == NULL)
    {
        __ERR("open file failed.\n");
        return pReturn;
    }

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);

    anj_mw_free(pCfgXml);

    if (pDocNode == NULL)
    {
        __ERR("parse file failed.\n");
        return pReturn;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "tz");
    if (pNodelist == NULL)
    {
        __ERR("xmlDocument_getElementsByTagName(tz) return NULL!\n");
        ixmlDocument_free(pDocNode);
        return pReturn;
    }

    IXML_Node *pNode = pNodelist->nodeItem;
    IXML_Node *pChildNode = pNode->firstChild;

    p = NULL;
    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "item"))
        {
            DstConfig *pData = (DstConfig *)anj_mw_malloc(sizeof(DstConfig));
            memset(pData, 0, sizeof(DstConfig));
            pData->nTimeZone = -1;

            IXML_Node *tmpAttr = NULL;
            tmpAttr = pChildNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "value"))
                {
                    StrCpy((char *)pData->szDstCfg, 64, tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            IXML_Node *pChildChild = pChildNode->firstChild;
            if (pChildChild)
            {
                char *nodeValue = ixmlNode_getNodeValue(pChildChild);
                if (NULL != nodeValue)
                {
                    StrCpy((char *)pData->szDstName, 64, nodeValue);
                }
            }

            if (strlen(pData->szDstCfg) == 0 && strlen(pData->szDstName) == 0)
            {
                anj_mw_free(pData);
            }
            else
            {
                if (pReturn == NULL)
                {
                    pReturn = pData;
                    p = pReturn;
                }
                else
                {
                    p->next = pData;
                    p = pData;
                }
            }
        }

        pChildNode = pChildNode->nextSibling;
    }

    ixmlNodeList_free(pNodelist);
    ixmlDocument_free(pDocNode);

    p = pReturn;
    while (p != NULL)
    {
        //__OUTPUTNONE("%-64s --> %-64s\n", p->szDstName, p->szDstCfg);
        char szDstName[64] = {0};
        strcpy(szDstName, p->szDstName);

        char *d1 = strstr(szDstName, "(");
        if (NULL != d1)
        {
            *d1 = 0;

            int timezone = 0, min = 0;
            char c;
            if (3 == sscanf(szDstName, "GMT%c%02d:%02d", &c, &timezone, &min))
            {
            }
            else if (2 == sscanf(szDstName, "GMT%c%02d", &c, &timezone))
            {
            }

            if (c == '-')
            {
                timezone = 0 - timezone;
                min = 0 - min;
            }

            p->nTimeZone = (timezone + 12) * 60 + min;

            char *m1 = strstr(p->szDstCfg, ",M");
            char *m2 = NULL;
            if (NULL != m1)
            {
                m1 += 1;
                m2 = strstr(m1, ",M");

                if (NULL != m2)
                {
                    *m2 = 0;
                    m2 += 1;
                }
            }

            if (m1 != NULL && m2 != NULL)
            {
                int month = 0, weekno = 0, weekday = 0, hour = 0;

                if (4 == sscanf(m1, "M%d.%d.%d/%d", &month, &weekno, &weekday, &hour))
                {
                }
                else if (3 == sscanf(m1, "M%d.%d.%d", &month, &weekno, &weekday))
                {
                }

                p->nStartMonth = month;
                p->nStartWeek = weekno;
                p->nStartWeekday = weekday;
                p->nStartHour = hour;

                hour = 0;

                if (4 == sscanf(m2, "M%d.%d.%d/%d", &month, &weekno, &weekday, &hour))
                {
                }
                else if (3 == sscanf(m2, "M%d.%d.%d", &month, &weekno, &weekday))
                {
                }

                p->nToMonth = month;
                p->nToWeek = weekno;
                p->nToWeekday = weekday;
                p->nToHour = hour;
            }

#if 0
            __OUTPUTNONE("\t %02d:%02d --> %d, from M%d.%d.%d/%d, to M%d.%d.%d/%d\n",
            timezone, min, p->nTimeZone,
            p->nStartMonth,
            p->nStartWeek,
            p->nStartWeekday,
            p->nStartHour,
            p->nToMonth,
            p->nToWeek,
            p->nToWeekday,
            p->nToHour);
#endif
        }

        p = p->next;
    }

    return pReturn;
}

void FreeSummerTimeList(DstConfig *head)
{
    DstConfig *p = head;
    while (p != NULL)
    {
        DstConfig *tmp = p;
        p = p->next;
        anj_mw_free(tmp);
    }
}

static int anj_config_system_ptz_dzoom_get(IXML_Node *pNode, DZoomConfig *pDZoomCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "multiple_max"))
        {
            pDZoomCfg->multiple_max = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "multiple_set"))
        {
            pDZoomCfg->multiple_set = Str2Double(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_ptz_af_get(IXML_Node *pNode, AfConfig *pAfCfg)
{
    pAfCfg->enable = 0;
    pAfCfg->type = 0;
    pAfCfg->bSendAFAlways = 1;
    pAfCfg->bSendCoordinate = 1;

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "enable"))
        {
            pAfCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "type"))
        {
            pAfCfg->type = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "bSendOnStart"))
        {
            pAfCfg->bSendAFAlways = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "bSendCoordinate"))
        {
            pAfCfg->bSendCoordinate = (short)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_ptz_func_get(IXML_Node *pNode, PTZFunction *pPtzFunction)
{
    memset(pPtzFunction, 0, sizeof(PTZFunction));
    pPtzFunction->reserveValue = -1;
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "FunctionName"))
        {
            memset(pPtzFunction->functionName, '\0', PTZ_FUNCTION_NAME_MAX_LEN);
            StrCpy(pPtzFunction->functionName, PTZ_FUNCTION_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PresetNumber"))
        {
            pPtzFunction->presetNum = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Function"))
        {
            memset(pPtzFunction->functionType.typeName, '\0', PTZ_FUNCTION_TYPE_MAX_LEN);
            StrCpy(pPtzFunction->functionType.typeName, PTZ_FUNCTION_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ReserveValue"))
        {
            pPtzFunction->reserveValue = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PresetNumber2"))
        {
            pPtzFunction->presetNum2 = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Function2"))
        {
            memset(pPtzFunction->functionType2.typeName, '\0', PTZ_FUNCTION_TYPE_MAX_LEN);
            StrCpy(pPtzFunction->functionType2.typeName, PTZ_FUNCTION_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "interval_sec"))
        {
            pPtzFunction->interval_sec = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (strcmp(pPtzFunction->functionName, "ScanOn") == 0)
    {
        if (pPtzFunction->reserveValue <= 0 || pPtzFunction->reserveValue > 100)
        {
            pPtzFunction->reserveValue = 50;
        }
    }
    else if (strcmp(pPtzFunction->functionName, "Orbit") == 0)
    {
        if (pPtzFunction->reserveValue < 0 || pPtzFunction->reserveValue > 60)
        {
            pPtzFunction->reserveValue = 10;
        }
    }

    return 0;
}

static int anj_config_system_ptz_advance_get(IXML_Node *pNode, PTZAdvanceConfig *pAdvanceCfg)
{
    int iChildCnt = 0;
    int iCurIdx = 0;

    IXML_Node *tmpChild = NULL;
    tmpChild = pNode->firstChild;

    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "FunctionConfig"))
        {
            anj_config_system_ptz_func_get(tmpChild, &(pAdvanceCfg->functions[iCurIdx]));
            iChildCnt++;
            iCurIdx++;
        }

        tmpChild = tmpChild->nextSibling;
    }

    pAdvanceCfg->functionCnt = iChildCnt;

    return 0;
}

static int anj_config_system_ptz_common_get(IXML_Node *pNode, PTZCommonConfig *pComCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Protocol"))
        {
            memset(pComCfg->ptzProtocol.protocolName, '\0', PTZ_PROTOCOL_NAME_MAX_LEN);
            StrCpy(pComCfg->ptzProtocol.protocolName, PTZ_PROTOCOL_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ComPort"))
        {
            pComCfg->comPort = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BaudRate"))
        {
            pComCfg->baudrate = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DataBits"))
        {
            pComCfg->dataBits = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StopBits"))
        {
            pComCfg->stopBits = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Verify"))
        {
            memset(pComCfg->verify.verifyName, '\0', PTZ_PROTOCOL_NAME_MAX_LEN);
            StrCpy(pComCfg->verify.verifyName, PTZ_PROTOCOL_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FlowControl"))
        {
            memset(pComCfg->flowControl.flowControlName, '\0', PTZ_PROTOCOL_NAME_MAX_LEN);
            StrCpy(pComCfg->flowControl.flowControlName, PTZ_PROTOCOL_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BootAction"))
        {
            pComCfg->bootAction = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_ptz_scan_get(IXML_Node *pNode, PTZScanConfig *pPtzScanCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "CruiseSpeed"))
        {
            pPtzScanCfg->cruiseSpeed = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "CruiseTime"))
        {
            pPtzScanCfg->cruiseTime = Str2Double(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LineScanTime"))
        {
            pPtzScanCfg->lineScanTime = Str2Double(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_alarmclock_oclock_get(IXML_Node *pNode, AlarmOClock *pAlarmOClockCfg)
{
    memset(pAlarmOClockCfg, 0, sizeof(AlarmClockConfig));
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "enable"))
        {
            pAlarmOClockCfg->enable = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "from_hour"))
        {
            pAlarmOClockCfg->from_hour = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "from_minute"))
        {
            pAlarmOClockCfg->from_minute = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "from_second"))
        {
            pAlarmOClockCfg->from_second = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "to_hour"))
        {
            pAlarmOClockCfg->to_hour = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "to_minute"))
        {
            pAlarmOClockCfg->to_minute = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "to_second"))
        {
            pAlarmOClockCfg->to_second = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_alarmclock_get(IXML_Node *pNode, AlarmClock *pAlarmClkCfg)
{
    int iCurIdx = 0;
    memset(pAlarmClkCfg, 0, sizeof(AlarmClock) * MAX_ALARM_CLOCK_NUM);
    IXML_Node *tmpChild = NULL;

    tmpChild = pNode->firstChild;

    while (tmpChild)
    {
        if (iCurIdx >= MAX_ALARM_CLOCK_NUM)
        {
            break;
        }

        AlarmClock *pOneClock = &pAlarmClkCfg[iCurIdx];
        IXML_Node *tmpAttr = NULL;

        tmpAttr = tmpChild->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "enable"))
            {
                pOneClock->enable = (unsigned char)Str2Num(tmpAttr->nodeValue);
            }
            else if (!strcmp(tmpAttr->nodeName, "hour"))
            {
                pOneClock->hour = (unsigned char)Str2Num(tmpAttr->nodeValue);
            }
            else if (!strcmp(tmpAttr->nodeName, "minute"))
            {
                pOneClock->minute = (unsigned char)Str2Num(tmpAttr->nodeValue);
            }
            else if (!strcmp(tmpAttr->nodeName, "second"))
            {
                pOneClock->second = (unsigned char)Str2Num(tmpAttr->nodeValue);
            }

            tmpAttr = tmpAttr->nextSibling;
        }

        tmpChild = tmpChild->nextSibling;
        iCurIdx++;
    }

    return 0;
}

static int anj_config_summer_time_get(IXML_Node *pNode, SummerTimeConfig *pSumTimeCfg)
{
    memset(pSumTimeCfg, 0, sizeof(SummerTimeConfig));

    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "enable"))
        {
            pSumTimeCfg->nEnable = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "auto"))
        {
            pSumTimeCfg->bAuto = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        if (!strcmp(tmpAttr->nodeName, "offset"))
        {
            pSumTimeCfg->nOffsetMin = (short)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (0 == strcmp(tmpChild->nodeName, "start"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (0 == strcmp(tmpAttr->nodeName, "month"))
                {
                    pSumTimeCfg->nStartMonth = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "week"))
                {
                    pSumTimeCfg->nStartWeek = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "weekday"))
                {
                    pSumTimeCfg->nStartWeekday = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "hour"))
                {
                    pSumTimeCfg->nStartHour = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }

        if (0 == strcmp(tmpChild->nodeName, "end"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (0 == strcmp(tmpAttr->nodeName, "month"))
                {
                    pSumTimeCfg->nToMonth = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "week"))
                {
                    pSumTimeCfg->nToWeek = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "weekday"))
                {
                    pSumTimeCfg->nToWeekday = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                if (0 == strcmp(tmpAttr->nodeName, "hour"))
                {
                    pSumTimeCfg->nToHour = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    __INFO("SummerTime enable %d, offset %d, from %u month %u week %u weekday %u hour to %u month %u week %u weekday %u hour\n",
          pSumTimeCfg->nEnable, pSumTimeCfg->nOffsetMin,
          pSumTimeCfg->nStartMonth, pSumTimeCfg->nStartWeek, pSumTimeCfg->nStartWeekday,
          pSumTimeCfg->nStartHour, pSumTimeCfg->nToMonth, pSumTimeCfg->nToWeek,
          pSumTimeCfg->nToWeekday, pSumTimeCfg->nToHour);

    return 0;
}

static int anj_config_user_account_get(IXML_Node *pNode, UserAccount *pUserAccount, UserConfig *pUserCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Username"))
        {
            memset(pUserAccount->userName, '\0', ACCOUNT_NAME_MAX_LEN);
            StrCpy(pUserAccount->userName, ACCOUNT_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Password"))
        {
            memset(pUserAccount->password, '\0', ACCOUNT_PASSWORD_MAX_LEN);
            StrCpy(pUserAccount->password, ACCOUNT_PASSWORD_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EncryptPwd"))
        {
            memset(pUserAccount->password, '\0', ACCOUNT_PASSWORD_MAX_LEN);

            char dst[64] = {0};
            char szEncryptData[128] = {0};
            StrCpy(szEncryptData, 128, tmpAttr->nodeValue);

            int ret = StringDecrypt(szEncryptData, dst, sizeof(dst));
            if (ret != 0)
            {
                __ERR("decrypt %s error.\n", szEncryptData);
            }
            else
            {
                char *tmpDst = restore_with_escape(dst);
                if (tmpDst != NULL)
                {
                    StrCpy(pUserAccount->password, ACCOUNT_PASSWORD_MAX_LEN, tmpDst);
                    anj_mw_free(tmpDst);
                    tmpDst = NULL;
                }
                else
                {
                    pUserAccount->password[0] = '\0';
                }
                __INFO("Get %s \n", pUserAccount->password);
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "Group"))
        {
            memset(pUserAccount->group.groupName, '\0', GROUP_NAME_MAX_LEN);
            StrCpy(pUserAccount->group.groupName, GROUP_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Status"))
        {
            memset(pUserAccount->status, '\0', ACCOUNT_STATUS_MAX_LEN);
            StrCpy(pUserAccount->status, ACCOUNT_STATUS_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "secureLoginMode"))
        {
            pUserCfg->secureLoginMode = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_ptz_get(IXML_Node *pNode, PTZConfig *pPtzCfg)
{
    anj_config_system_ptz_common_get(pNode, &(pPtzCfg->commonCfg));

    IXML_Node *tmpChild = NULL;
    tmpChild = pNode->firstChild;

    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AdvanceConfig"))
        {
            anj_config_system_ptz_advance_get(tmpChild, &(pPtzCfg->advanceCfg));
        }
        else if (!strcmp(tmpChild->nodeName, "AfConfig"))
        {
            anj_config_system_ptz_af_get(tmpChild, &pPtzCfg->afCfg);
        }
        else if (!strcmp(tmpChild->nodeName, "DzoomConfig"))
        {
            anj_config_system_ptz_dzoom_get(tmpChild, &pPtzCfg->dzoomCfg);
        }
        else if (!strcmp(tmpChild->nodeName, "ScanConfig"))
        {
            anj_config_system_ptz_scan_get(tmpChild, &pPtzCfg->scanConfig);
        }

        tmpChild = tmpChild->nextSibling;
    }
    return 0;
}

static int anj_config_system_time_get(IXML_Node *pNode, TimeConfig *pTimeCfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;

    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "TimeMode"))
        {
            memset(pTimeCfg->timeMode.modeName, '\0', TIME_MODE_MAX_LEN);
            StrCpy(pTimeCfg->timeMode.modeName, TIME_MODE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TimeZone"))
        {
            pTimeCfg->timeZone = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    IXML_Node *tmpChild = NULL;
    tmpChild = pNode->firstChild;

    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "NTPConfig"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "ServerIP"))
                {
                    memset(pTimeCfg->ntpConfig.serverIP, '\0', MAX_IP_NAME_LEN);
                    StrCpy(pTimeCfg->ntpConfig.serverIP, MAX_IP_NAME_LEN, tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "ServerPort"))
                {
                    pTimeCfg->ntpConfig.serverPort = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "RefreshInterval"))
                {
                    pTimeCfg->ntpConfig.refreshInterval = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }
        }

        if (!strcmp(tmpChild->nodeName, "SummerTime"))
        {
            anj_config_summer_time_get(tmpChild, &pTimeCfg->summerConfig);
        }

        tmpChild = tmpChild->nextSibling;
    }

    if (pTimeCfg->timeZone < 0)
        pTimeCfg->timeZone = 1200;

    if (
        (0 == pTimeCfg->summerConfig.nStartMonth &&
         0 == pTimeCfg->summerConfig.nStartWeek &&
         0 == pTimeCfg->summerConfig.nStartWeekday &&
         0 == pTimeCfg->summerConfig.nToMonth &&
         0 == pTimeCfg->summerConfig.nToWeek &&
         0 == pTimeCfg->summerConfig.nToWeekday) ||
        (1 == pTimeCfg->summerConfig.nStartMonth &&
         1 == pTimeCfg->summerConfig.nStartWeek &&
         0 == pTimeCfg->summerConfig.nStartWeekday &&
         1 == pTimeCfg->summerConfig.nToMonth &&
         1 == pTimeCfg->summerConfig.nToWeek &&
         0 == pTimeCfg->summerConfig.nToWeekday))
    {
        DstConfig *pHead = ReadSummerTimeListFromXML();
        DstConfig *p = pHead;
        while (p != NULL)
        {
            DstConfig *tmp = p;

            if (tmp->nTimeZone >= 0 && tmp->nTimeZone == pTimeCfg->timeZone)
            {
                pTimeCfg->summerConfig.nStartMonth = tmp->nStartMonth;
                pTimeCfg->summerConfig.nStartWeek = tmp->nStartWeek;
                pTimeCfg->summerConfig.nStartWeekday = tmp->nStartWeekday;
                pTimeCfg->summerConfig.nStartHour = tmp->nStartHour;
                pTimeCfg->summerConfig.nToMonth = tmp->nToMonth;
                pTimeCfg->summerConfig.nToWeek = tmp->nToWeek;
                pTimeCfg->summerConfig.nToWeekday = tmp->nToWeekday;
                pTimeCfg->summerConfig.nToHour = tmp->nToHour;
                break;
            }
            p = p->next;
        }

        FreeSummerTimeList(pHead);
    }

    return 0;
}

static int anj_config_system_user_get(IXML_Node *pNode, UserConfig *pUserCfg)
{
    IXML_Node *tmpChild = NULL;
    int iChildCnt = 0;
    int iCurIdx = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (iChildCnt >= MAX_ACCOUNT_COUNT) // added by XXX 20090514
            break;

        if (!strcmp(tmpChild->nodeName, "Account"))
        {
            anj_config_user_account_get(tmpChild, &(pUserCfg->accounts[iCurIdx]), pUserCfg);
            iChildCnt++;
            iCurIdx++;
        }

        tmpChild = tmpChild->nextSibling;
    }

    pUserCfg->count = iChildCnt;

    return 0;
}

static int anj_config_system_syslog_get(IXML_Node *pNode, SyslogConfig *pSyslogCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "LogLevel"))
        {
            memset(pSyslogCfg->logLevel.levelName, '\0', LOG_LEVEL_NAME_MAX_LEN);
            StrCpy(pSyslogCfg->logLevel.levelName, LOG_LEVEL_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MaxDay"))
        {
            pSyslogCfg->maxDays = Str2Num(tmpAttr->nodeValue);
        }
        /*
        else if(!strcmp(tmpAttr->nodeName, "MaxEventPerday"))
        {
        pSyslogCfg->maxEventPerday= Str2Num(tmpAttr->nodeValue);
        }
        */
        else if (!strcmp(tmpAttr->nodeName, "StoreMedia"))
        {
            memset(pSyslogCfg->storeMedia.mediaName, '\0', STORE_MEDIA_NAME_MAX_LEN);
            StrCpy(pSyslogCfg->storeMedia.mediaName, STORE_MEDIA_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StorePolicy"))
        {
            memset(pSyslogCfg->storePolicy.policyName, '\0', STORE_POLICY_NAME_MAX_LEN);
            StrCpy(pSyslogCfg->storePolicy.policyName, STORE_POLICY_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "AutoBackup"))
        {
            pSyslogCfg->autoBackup = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BackupWay"))
        {
            memset(pSyslogCfg->backupWay.wayName, '\0', BACKUP_WAY_NAME_MAX_LEN);
            StrCpy(pSyslogCfg->backupWay.wayName, BACKUP_WAY_NAME_MAX_LEN, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_misc_get(IXML_Node *pNode, MiscConfig *pMiscCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Language"))
        {
            memset(pMiscCfg->language, '\0', MAX_LANGUAGE_LEN);
            StrCpy(pMiscCfg->language, MAX_LANGUAGE_LEN, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_maintain_get(IXML_Node *pNode, MaintainConfig *pMaintainCfg)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pMaintainCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Day"))
        {
            pMaintainCfg->day = Str2Num(tmpAttr->nodeValue);
            if (pMaintainCfg->day > 7 || pMaintainCfg->day < 0) // 0-6 = monday - sunday, 7 = everyday
            {
                pMaintainCfg->day = 7;
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "Time"))
        {
            GetTime(tmpAttr->nodeValue, &(pMaintainCfg->time));
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

int anj_config_system_allowip_get(IXML_Node *pNode, SysAlowIpConfig *pSysAllowIpCfg)
{
    memset(pSysAllowIpCfg, 0, sizeof(SysAlowIpConfig));

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pSysAllowIpCfg->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ipaddr1"))
        {
            pSysAllowIpCfg->nAllowIp[0] = CheckAtoU(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ipaddr2"))
        {
            pSysAllowIpCfg->nAllowIp[1] = CheckAtoU(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ipaddr3"))
        {
            pSysAllowIpCfg->nAllowIp[2] = CheckAtoU(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ipaddr4"))
        {
            pSysAllowIpCfg->nAllowIp[3] = CheckAtoU(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ipaddr5"))
        {
            pSysAllowIpCfg->nAllowIp[4] = CheckAtoU(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (pSysAllowIpCfg->enable && pSysAllowIpCfg->nAllowIp[0] == 0 && pSysAllowIpCfg->nAllowIp[1] == 0 && pSysAllowIpCfg->nAllowIp[2] == 0 && pSysAllowIpCfg->nAllowIp[3] == 0)
    {
        pSysAllowIpCfg->enable = 0;
    }

    return 0;
}

static int anj_config_system_alarmclock_get(IXML_Node *pNode, AlarmClockConfig *pAlarmClockCfg)
{
    memset(pAlarmClockCfg, 0, sizeof(AlarmClockConfig));

    IXML_Node *tmpChild = pNode->firstChild;
    while (tmpChild != NULL)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmOnEveryHour"))
        {
            anj_config_alarmclock_oclock_get(tmpChild, &(pAlarmClockCfg->oclock));
        }
        else if (!strcmp(tmpChild->nodeName, "alarmclock"))
        {
            anj_config_alarmclock_get(tmpChild, (pAlarmClockCfg->alarmclock));
        }

        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

void anj_config_system_videoq_default(VideoQoSConfig *pVideoqCfg)
{
    memset(pVideoqCfg, 0, sizeof(VideoQoSConfig));
    pVideoqCfg->adjust_bitrate = 1;
    pVideoqCfg->adjust_fps = 1;
    pVideoqCfg->enable_by_network = 1;
    pVideoqCfg->enable_by_sdcard = 1;
    pVideoqCfg->enable_by_cloud = 1;
    pVideoqCfg->enable_by_sdcardandcloud = 1;
    pVideoqCfg->enable_by_cloudstorage = 1;
}

static int anj_config_system_videoq_get(IXML_Node *pNode, VideoQoSConfig *pVideoqCfg)
{
    anj_config_system_videoq_default(pVideoqCfg);

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "adjust_bitrate"))
        {
            pVideoqCfg->adjust_bitrate = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "adjust_fps"))
        {
            pVideoqCfg->adjust_fps = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "by_network"))
        {
            pVideoqCfg->enable_by_network = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "by_sdcard"))
        {
            pVideoqCfg->enable_by_sdcard = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "by_cloud"))
        {
            pVideoqCfg->enable_by_cloud = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "by_sdcardandcloud"))
        {
            pVideoqCfg->enable_by_sdcardandcloud = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "by_cloudstorage"))
        {
            pVideoqCfg->enable_by_cloudstorage = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_audioprompt_get(IXML_Node *pNode, AudioPromptConfig *pAudioPromptcfg)
{
    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "startup"))
        {
            pAudioPromptcfg->startup = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ota"))
        {
            pAudioPromptcfg->ota = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "network"))
        {
            pAudioPromptcfg->network = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "sdcard"))
        {
            pAudioPromptcfg->sdcard = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "reset"))
        {
            pAudioPromptcfg->reset = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_tamperproof_get(IXML_Node *pNode, TamperProofConfig *pTamperCfg)
{
    memset(pTamperCfg, 0, sizeof(TamperProofConfig));

    if (access("/opt/ch/flag.modifymac", F_OK) == 0)
    {
        pTamperCfg->mac = 0;
    }
    else
    {
        pTamperCfg->mac = 1;
        pTamperCfg->network = 0;
    }

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "mac"))
        {
            pTamperCfg->mac = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "network"))
        {
            pTamperCfg->network = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "dhcpOnReboot"))
        {
            pTamperCfg->dhcpOnReboot = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_system_location_get(IXML_Node *pNode, LocationConfig *pCfg)
{
    memset(pCfg, 0, sizeof(LocationConfig));

    pCfg->push_reset = 0;

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PushReset"))
        {
            pCfg->push_reset = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

static int anj_config_system_streetlamp_get(IXML_Node *pNode, StreetLampConfig *pCfg)
{
    memset(pCfg, 0, sizeof(StreetLampConfig));
    pCfg->mode = 0;
    pCfg->manualEnable = 0;
    pCfg->openSensitivity = 50;
    pCfg->closeSensitivity = 50;

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "mode"))
        {
            pCfg->mode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "manualEnable"))
        {
            pCfg->manualEnable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "openSensitivity"))
        {
            pCfg->openSensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "closeSensitivity"))
        {
            pCfg->closeSensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "nightStartTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pCfg->nightStartTime.startTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "nightEndTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pCfg->nightEndTime.endTime));
        }

        tmpAttr = tmpAttr->nextSibling;
    }
    return 0;
}

char *anj_config_system_ptz_af_conver_xml(AfConfig *pPtzCfg)
{
    char *buf = NULL;
    char *pe = NULL;
    char *pb = NULL;
    int bufSize = 512;

    buf = (char *)anj_mw_malloc(bufSize);
    memset(buf, '\0', bufSize);
    pb = buf;
    pe = buf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<AfConfig ");
    pb += snprintf(pb, pe - pb, "enable=\"%d\" ", pPtzCfg->enable);
    pb += snprintf(pb, pe - pb, "type=\"%d\" ", pPtzCfg->type);
    pb += snprintf(pb, pe - pb, "bSendOnStart=\"%d\" ", pPtzCfg->bSendAFAlways);
    pb += snprintf(pb, pe - pb, "bSendCoordinate=\"%d\" ", pPtzCfg->bSendCoordinate);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_ptz_dzoom_conver_xml(DZoomConfig *pPtzCfg)
{
    char *buf = NULL;
    char *pe = NULL;
    char *pb = NULL;
    int bufSize = 512;

    buf = (char *)anj_mw_malloc(bufSize);
    memset(buf, '\0', bufSize);
    pb = buf;
    pe = buf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<DzoomConfig ");
    pb += snprintf(pb, pe - pb, "multiple_max=\"%.1f\" ", pPtzCfg->multiple_max);
    pb += snprintf(pb, pe - pb, "multiple_set=\"%.1f\" ", pPtzCfg->multiple_set);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_system_ptz_scan_conver_xml(PTZScanConfig *pPtzScanCfg)
{
    char *buf = NULL;
    char *pe = NULL;
    char *pb = NULL;
    int bufSize = 512;

    buf = (char *)anj_mw_malloc(bufSize);
    memset(buf, '\0', bufSize);
    pb = buf;
    pe = buf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<ScanConfig ");
    pb += snprintf(pb, pe - pb, "CruiseSpeed=\"%d\" ", pPtzScanCfg->cruiseSpeed);
    pb += snprintf(pb, pe - pb, "CruiseTime=\"%d\" ", pPtzScanCfg->cruiseTime);
    pb += snprintf(pb, pe - pb, "LineScanTime=\"%d\" ", pPtzScanCfg->lineScanTime);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    return buf;
}

char *anj_config_system_ptz_advance_conver_xml(PTZAdvanceConfig *pPtzConfig)
{
    char *buf = NULL;
    char *pe = NULL;
    char *pb = NULL;
    int sizePerFunc = 200;
    int bufSize = 0;
    char escapeBuf[1000] = {0};

    bufSize = pPtzConfig->functionCnt * sizePerFunc;
    buf = (char *)anj_mw_malloc(bufSize);
    memset(buf, '\0', bufSize);
    pb = buf;
    pe = buf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<AdvanceConfig>\r\n");

    int i = 0;
    for (; i < pPtzConfig->functionCnt; i++)
    {
        pb += snprintf(pb, pe - pb, "<FunctionConfig\r\n");
        pb += snprintf(pb, pe - pb, "FunctionName=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->functions[i].functionName));
        pb += snprintf(pb, pe - pb, "PresetNumber=\"%d\"\r\n", pPtzConfig->functions[i].presetNum);
        pb += snprintf(pb, pe - pb, "Function=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->functions[i].functionType.typeName));

        if (strcmp(pPtzConfig->functions[i].functionName, "ScanOn") == 0 || strcmp(pPtzConfig->functions[i].functionName, "Orbit") == 0)
        {
            pb += snprintf(pb, pe - pb, "ReserveValue=\"%d\"\r\n", pPtzConfig->functions[i].reserveValue);
        }

        pb += snprintf(pb, pe - pb, "PresetNumber2=\"%d\"\r\n", pPtzConfig->functions[i].presetNum2);
        pb += snprintf(pb, pe - pb, "Function2=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->functions[i].functionType2.typeName));
        pb += snprintf(pb, pe - pb, "interval_sec=\"%d\"\r\n", pPtzConfig->functions[i].interval_sec);

        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</AdvanceConfig>\r\n");

    return buf;
}

char *anj_config_alarm_clock_conver_xml(AlarmOClock *pCfg)
{
    int maxSize = 1024;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<AlarmOnEveryHour ");
    pb += snprintf(pb, pe - pb, "enable=\"%d\" ", pCfg->enable);
    pb += snprintf(pb, pe - pb, "from_hour=\"%d\" ", pCfg->from_hour);
    pb += snprintf(pb, pe - pb, "from_minute=\"%d\" ", pCfg->from_minute);
    pb += snprintf(pb, pe - pb, "from_second=\"%d\" ", pCfg->from_second);
    pb += snprintf(pb, pe - pb, "to_hour=\"%d\" ", pCfg->to_hour);
    pb += snprintf(pb, pe - pb, "to_minute=\"%d\" ", pCfg->to_minute);
    pb += snprintf(pb, pe - pb, "to_second=\"%d\" ", pCfg->to_second);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_alarm_clock_list_conver_xml(AlarmClock *pCfgList)
{
    int maxSize = 1000;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    int i = 0;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<alarmclock>\r\n");

    for (i = 0; i < MAX_ALARM_CLOCK_NUM; i++)
    {
        AlarmClock *pData = &pCfgList[i];

        pb += snprintf(pb, pe - pb, "<clock ");
        pb += snprintf(pb, pe - pb, "enable=\"%d\" ", pData->enable);
        pb += snprintf(pb, pe - pb, "hour=\"%d\" ", pData->hour);
        pb += snprintf(pb, pe - pb, "minute=\"%d\" ", pData->minute);
        pb += snprintf(pb, pe - pb, "second=\"%d\" ", pData->second);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }
    pb += snprintf(pb, pe - pb, "</alarmclock>\r\n");

    return buf;
}

char *anj_config_system_ptz_conver_xml(PTZConfig *pPtzConfig)
{
    int incrSize = 0;
    int initSize = 512;
    char *tmp = NULL;
    char escapeBuf[1000] = {0};

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<PTZConfig\r\n");
    pb += snprintf(pb, pe - pb, "Protocol=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->commonCfg.ptzProtocol.protocolName));
    pb += snprintf(pb, pe - pb, "ComPort=\"%d\"\r\n", pPtzConfig->commonCfg.comPort);
    pb += snprintf(pb, pe - pb, "BaudRate=\"%d\"\r\n", pPtzConfig->commonCfg.baudrate);
    pb += snprintf(pb, pe - pb, "DataBits=\"%d\"\r\n", pPtzConfig->commonCfg.dataBits);
    pb += snprintf(pb, pe - pb, "StopBits=\"%d\"\r\n", pPtzConfig->commonCfg.stopBits);
    pb += snprintf(pb, pe - pb, "Verify=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->commonCfg.verify.verifyName));
    pb += snprintf(pb, pe - pb, "FlowControl=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzConfig->commonCfg.flowControl.flowControlName));
    pb += snprintf(pb, pe - pb, "BootAction=\"%d\"\r\n", pPtzConfig->commonCfg.bootAction); // XXX 20110509
    pb += snprintf(pb, pe - pb, ">\r\n");

    curPos = pb - buf;

    tmp = anj_config_system_ptz_af_conver_xml(&(pPtzConfig->afCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    tmp = anj_config_system_ptz_dzoom_conver_xml(&(pPtzConfig->dzoomCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    tmp = anj_config_system_ptz_scan_conver_xml(&(pPtzConfig->scanConfig));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_ptz_advance_conver_xml(&(pPtzConfig->advanceCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</PTZConfig>\r\n");

    return buf;
}

char *anj_config_system_time_conver_xml(TimeConfig *pTimeCfg)
{
    int maxSize = 1024;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    char escapeBuf[1000] = {0};

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<TimeConfig TimeMode=\"%s\" TimeZone=\"%d\" >\r\n",
                   copy_with_escape(escapeBuf, pTimeCfg->timeMode.modeName),
                   pTimeCfg->timeZone);

    pb += snprintf(pb, pe - pb, "<NTPConfig ServerIP=\"%s\" ServerPort=\"%d\" RefreshInterval=\"%d\" />\r\n",
                   copy_with_escape(escapeBuf, pTimeCfg->ntpConfig.serverIP),
                   pTimeCfg->ntpConfig.serverPort,
                   pTimeCfg->ntpConfig.refreshInterval);

    SummerTimeConfig *p = &pTimeCfg->summerConfig;
    pb += snprintf(pb, pe - pb, "<SummerTime enable=\"%d\" auto=\"%d\" offset=\"%d\" >\r\n",
                   p->nEnable, p->bAuto, p->nOffsetMin);
    pb += snprintf(pb, pe - pb, "<start month=\"%d\" week=\"%d\" weekday=\"%d\" hour=\"%d\" />\r\n",
                   p->nStartMonth, p->nStartWeek, p->nStartWeekday, p->nStartHour);
    pb += snprintf(pb, pe - pb, "<end month=\"%d\" week=\"%d\" weekday=\"%d\" hour=\"%d\" />\r\n",
                   p->nToMonth, p->nToWeek, p->nToWeekday, p->nToHour);
    pb += snprintf(pb, pe - pb, "</SummerTime>\r\n");
    pb += snprintf(pb, pe - pb, "</TimeConfig>\r\n");

    return buf;
}

char *anj_config_system_user_password_conver_xml(UserConfig *pUserCfg)
{
    char *pBuf = NULL;
    int i = 0;
    int sizePerAccount = 200;
    int totalSize = pUserCfg->count * sizePerAccount + 50;
    char md5Str[64];

    pBuf = (char *)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<UserConfig>\r\n");

    for (i = 0; i < pUserCfg->count; i++)
    {
        pb += snprintf(pb, pe - pb, "<Account\r\n");
        pb += snprintf(pb, pe - pb, "Username=\"%s\"\r\n", pUserCfg->accounts[i].userName);

        our_md5_encode(md5Str, (unsigned char *)pUserCfg->accounts[i].password, strlen(pUserCfg->accounts[i].password));
        md5Str[32] = '\0';

        pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", md5Str);

        pb += snprintf(pb, pe - pb, "Group=\"%s\"\r\n", pUserCfg->accounts[i].group.groupName);
        pb += snprintf(pb, pe - pb, "Status=\"%s\"\r\n", pUserCfg->accounts[i].status);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</UserConfig>\r\n");

    return pBuf;
}

char *anj_config_system_user_conver_xml(UserConfig *pUserCfg, int bPwdEntrypt)
{
    char *pBuf = NULL;
    int i = 0;
    int sizePerAccount = 200;
    int totalSize = 0;
    char escapeBuf[1000];

    totalSize = pUserCfg->count * sizePerAccount + 50;
    pBuf = (char *)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<UserConfig>\r\n");

    for (i = 0; i < pUserCfg->count; i++)
    {
        pb += snprintf(pb, pe - pb, "<Account ");
        pb += snprintf(pb, pe - pb, "Username=\"%s\" ", copy_with_escape(escapeBuf, pUserCfg->accounts[i].userName));

        copy_with_escape(escapeBuf, pUserCfg->accounts[i].password);
        char dst[64] = {0};

        int ret = StringEncrypt(escapeBuf, dst, sizeof(dst));
        if (0 == bPwdEntrypt || ret != 0)
        {
            if (ret != 0)
            {
                __ERR("Encrypt %s error.", escapeBuf);
            }
            pb += snprintf(pb, pe - pb, "Password=\"%s\"\r\n", escapeBuf);
        }
        else
        {
            pb += snprintf(pb, pe - pb, "EncryptPwd=\"%s\"\r\n", dst);
        }

        pb += snprintf(pb, pe - pb, "Group=\"%s\" ", copy_with_escape(escapeBuf, pUserCfg->accounts[i].group.groupName));
        pb += snprintf(pb, pe - pb, "Status=\"%s\" ", copy_with_escape(escapeBuf, pUserCfg->accounts[i].status));
        pb += snprintf(pb, pe - pb, "secureLoginMode=\"%d\"\r\n", pUserCfg->secureLoginMode);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</UserConfig>\r\n");

    return pBuf;
}

char *anj_config_system_syslog_conver_xml(SyslogConfig *pSyslogCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    char escapeBuf[1000] = {0};

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<SystemLogConfig\r\n");
    pb += snprintf(pb, pe - pb, "LogLevel=\"%s\"\r\n", copy_with_escape(escapeBuf, pSyslogCfg->logLevel.levelName));
    pb += snprintf(pb, pe - pb, "MaxDay=\"%d\"\r\n", pSyslogCfg->maxDays);
    // pb += snprintf(pb, pe-pb, "MaxEventPerday=\"%d\"\r\n", pSyslogCfg->maxEventPerday);
    pb += snprintf(pb, pe - pb, "StoreMedia=\"%s\"\r\n", copy_with_escape(escapeBuf, pSyslogCfg->storeMedia.mediaName));
    pb += snprintf(pb, pe - pb, "StorePolicy=\"%s\"\r\n", copy_with_escape(escapeBuf, pSyslogCfg->storePolicy.policyName));
    pb += snprintf(pb, pe - pb, "AutoBackup=\"%d\"\r\n", pSyslogCfg->autoBackup);
    pb += snprintf(pb, pe - pb, "BackupWay=\"%s\"\r\n", copy_with_escape(escapeBuf, pSyslogCfg->backupWay.wayName));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_misc_conver_xml(MiscConfig *pMiscCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    char escapeBuf[1000] = {0};

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<MiscConfig\r\n");
    pb += snprintf(pb, pe - pb, "Language=\"%s\"\r\n", copy_with_escape(escapeBuf, pMiscCfg->language));
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_maintain_conver_xml(MaintainConfig *pMaintainCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<MaintainConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pMaintainCfg->enable);
    pb += snprintf(pb, pe - pb, "Day=\"%d\"\r\n", pMaintainCfg->day);
    pb += snprintf(pb, pe - pb, "Time=\"%2d:%2d:%2d\"\r\n",
                   pMaintainCfg->time.hour,
                   pMaintainCfg->time.minute,
                   pMaintainCfg->time.sec);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_allowip_conver_xml(SysAlowIpConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<AlowIpConfig\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pCfg->enable);

    int iIndex = 0;
    for (iIndex = 0; iIndex < MAX_ALOW_IP_NUM; iIndex++)
    {
        //		if( pCfg->nAllowIp[iIndex] > 0 )
        {
            pb += snprintf(pb, pe - pb, "ipaddr%d=\"%u\"\r\n", iIndex + 1, pCfg->nAllowIp[iIndex]);
        }
    }

    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_alarmclock_conver_xml(AlarmClockConfig *pCfg)
{
    int initSize = 100;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<AlarmClockConfig>\r\n");

    curPos = pb - buf;
    tmp = anj_config_alarm_clock_conver_xml(&pCfg->oclock);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_alarm_clock_list_conver_xml(pCfg->alarmclock);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</AlarmClockConfig>\r\n");

    return buf;
}

char *anj_config_system_videoq_conver_xml(VideoQoSConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<VideoQoSConfig\r\n");
    pb += snprintf(pb, pe - pb, "adjust_bitrate=\"%u\"\r\n", pCfg->adjust_bitrate);
    pb += snprintf(pb, pe - pb, "adjust_fps=\"%u\"\r\n", pCfg->adjust_fps);
    pb += snprintf(pb, pe - pb, "by_network=\"%u\"\r\n", pCfg->enable_by_network);
    pb += snprintf(pb, pe - pb, "by_sdcard=\"%u\"\r\n", pCfg->enable_by_sdcard);
    pb += snprintf(pb, pe - pb, "by_cloud=\"%u\"\r\n", pCfg->enable_by_cloud);
    pb += snprintf(pb, pe - pb, "by_sdcardandcloud=\"%u\"\r\n", pCfg->enable_by_sdcardandcloud);
    pb += snprintf(pb, pe - pb, "by_cloudstorage=\"%u\"\r\n", pCfg->enable_by_cloudstorage);

    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_audioprompt_conver_xml(AudioPromptConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<AudioPromptConfig\r\n");
    pb += snprintf(pb, pe - pb, "startup=\"%u\"\r\n", pCfg->startup);
    pb += snprintf(pb, pe - pb, "ota=\"%u\"\r\n", pCfg->ota);
    pb += snprintf(pb, pe - pb, "network=\"%u\"\r\n", pCfg->network);
    pb += snprintf(pb, pe - pb, "sdcard=\"%u\"\r\n", pCfg->sdcard);
    pb += snprintf(pb, pe - pb, "reset=\"%u\"\r\n", pCfg->reset);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_tamperproof_conver_xml(TamperProofConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<TamperProofConfig\r\n");
    pb += snprintf(pb, pe - pb, "mac=\"%u\"\r\n", pCfg->mac);
    pb += snprintf(pb, pe - pb, "network=\"%u\"\r\n", pCfg->network);
    pb += snprintf(pb, pe - pb, "dhcpOnReboot=\"%u\"\r\n", pCfg->dhcpOnReboot);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_location_conver_xml(LocationConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<LocationConfig\r\n");
    pb += snprintf(pb, pe - pb, "PushReset=\"%d\"\r\n", pCfg->push_reset);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_streetlamp_conver_xml(StreetLampConfig *pCfg)
{
    int maxSize = 500;
    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<StreetLamp\r\n");
    pb += snprintf(pb, pe - pb, "mode=\"%d\"\r\n", pCfg->mode);
    pb += snprintf(pb, pe - pb, "manualEnable=\"%d\"\r\n", pCfg->manualEnable);
    pb += snprintf(pb, pe - pb, "openSensitivity=\"%d\"\r\n", pCfg->openSensitivity);
    pb += snprintf(pb, pe - pb, "closeSensitivity=\"%d\"\r\n", pCfg->closeSensitivity);
    pb += snprintf(pb, pe - pb, "nightStartTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->nightStartTime.startTime.hour,
                   pCfg->nightStartTime.startTime.minute,
                   pCfg->nightStartTime.startTime.sec);
    pb += snprintf(pb, pe - pb, "nightEndTime=\"%02d:%02d:%02d\"\r\n",
                   pCfg->nightEndTime.endTime.hour,
                   pCfg->nightEndTime.endTime.minute,
                   pCfg->nightEndTime.endTime.sec);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_system_conver_xml(SystemConfig *pSystemCfg)
{
    int incrSize = 0;
    int initSize = 1000;
    char *tmp = NULL;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<SystemConfig>\r\n");

    curPos = pb - buf;
    tmp = anj_config_system_ptz_conver_xml(&(pSystemCfg->ptzCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_time_conver_xml(&(pSystemCfg->timeCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_user_conver_xml(&(pSystemCfg->userCfg), 1);
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_syslog_conver_xml(&(pSystemCfg->syslogCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_misc_conver_xml(&(pSystemCfg->miscCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_maintain_conver_xml(&(pSystemCfg->maintainCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_allowip_conver_xml(&(pSystemCfg->alowipCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_alarmclock_conver_xml(&(pSystemCfg->clockSetting));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_streetlamp_conver_xml(&(pSystemCfg->streetLampCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_videoq_conver_xml(&(pSystemCfg->videoQosCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_audioprompt_conver_xml(&(pSystemCfg->audioPromptCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_tamperproof_conver_xml(&(pSystemCfg->tamperProofCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - buf;
    tmp = anj_config_system_location_conver_xml(&(pSystemCfg->locationCfg));
    incrSize = strlen(tmp);
    buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
    curSize = curSize + incrSize;
    memcpy(buf + curPos, tmp, strlen(tmp));
    curPos = curPos + strlen(tmp);
    pb = buf + curPos;
    pe = buf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</SystemConfig>\r\n");

    return buf;
}

int anj_config_system_get(IXML_Node *pNode, SystemConfig *pSystemCfg)
{
    IXML_Node *pChildNode = pNode->firstChild;

    memset(pSystemCfg, 0, sizeof(SystemConfig));
    pSystemCfg->audioPromptCfg.network = 0;
    pSystemCfg->audioPromptCfg.ota = 0;
    pSystemCfg->audioPromptCfg.sdcard = 0;
    pSystemCfg->audioPromptCfg.startup = 0;
    pSystemCfg->audioPromptCfg.reset = 1;
    pSystemCfg->streetLampCfg.openSensitivity = 60;
    pSystemCfg->streetLampCfg.closeSensitivity = 60;
    pSystemCfg->streetLampCfg.nightStartTime.startTime.hour = 18;
    pSystemCfg->streetLampCfg.nightEndTime.endTime.hour = 8;
    anj_config_system_videoq_default(&pSystemCfg->videoQosCfg);

    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "PTZConfig"))
        {
            anj_config_system_ptz_get(pChildNode, &(pSystemCfg->ptzCfg));
        }
        else if (!strcmp(pChildNode->nodeName, "TimeConfig"))
        {
            anj_config_system_time_get(pChildNode, &pSystemCfg->timeCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "UserConfig"))
        {
            anj_config_system_user_get(pChildNode, &pSystemCfg->userCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "SystemLogConfig"))
        {
            anj_config_system_syslog_get(pChildNode, &pSystemCfg->syslogCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "MiscConfig"))
        {
            anj_config_system_misc_get(pChildNode, &pSystemCfg->miscCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "MaintainConfig"))
        {
            anj_config_system_maintain_get(pChildNode, &pSystemCfg->maintainCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "AlowIpConfig"))
        {
            anj_config_system_allowip_get(pChildNode, &pSystemCfg->alowipCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "AlarmClockConfig"))
        {
            anj_config_system_alarmclock_get(pChildNode, &pSystemCfg->clockSetting);
        }
        else if (!strcmp(pChildNode->nodeName, "VideoQoSConfig"))
        {
            anj_config_system_videoq_get(pChildNode, &pSystemCfg->videoQosCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "AudioPromptConfig"))
        {
            anj_config_system_audioprompt_get(pChildNode, &pSystemCfg->audioPromptCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "StreetLamp"))
        {
            anj_config_system_streetlamp_get(pChildNode, &pSystemCfg->streetLampCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "TamperProofConfig"))
        {
            anj_config_system_tamperproof_get(pChildNode, &pSystemCfg->tamperProofCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "LocationConfig"))
        {
            anj_config_system_location_get(pChildNode, &pSystemCfg->locationCfg);
        }

        pChildNode = pChildNode->nextSibling;
    }

    return 0;
}

int anj_config_system_save(SystemConfig *pSystemCfg)
{
    int iRet = 0;

    char *pDataXml = anj_config_system_conver_xml(pSystemCfg);
    iRet = anj_config_save_node(pDataXml, "<SystemConfig>", "</SystemConfig>");

    anj_mw_free(pDataXml);
    pDataXml = NULL;

    return iRet;
}

int anj_config_system_set(SystemConfig *pSystemCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();

    if (memcmp(systemcfg, pSystemCfg, sizeof(SystemConfig)))
    {
        // Todo
        {
            memcpy(systemcfg, pSystemCfg, sizeof(SystemConfig));
            anj_config_system_save(systemcfg);
        }
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_set(PTZConfig *pPtzCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    PTZConfig *ptzcfg = &systemcfg->ptzCfg;

    if (memcmp(ptzcfg, pPtzCfg, sizeof(PTZConfig)))
    {
        // todo ...
        memcpy(ptzcfg, pPtzCfg, sizeof(PTZConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_comm_set(PTZCommonConfig *pPtzCommCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    PTZCommonConfig *ptzcommcfg = &systemcfg->ptzCfg.commonCfg;

    if (memcmp(ptzcommcfg, pPtzCommCfg, sizeof(PTZCommonConfig)))
    {
        // todo ...
        memcpy(ptzcommcfg, pPtzCommCfg, sizeof(PTZCommonConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_advance_set(PTZAdvanceConfig *pPtzAdvanceCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    PTZAdvanceConfig *advancecfg = &systemcfg->ptzCfg.advanceCfg;

    if (memcmp(advancecfg, pPtzAdvanceCfg, sizeof(PTZAdvanceConfig)))
    {
        // todo ...
        memcpy(advancecfg, pPtzAdvanceCfg, sizeof(PTZAdvanceConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_af_set(AfConfig *pAfCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    AfConfig *afcfg = &systemcfg->ptzCfg.afCfg;

    if (memcmp(afcfg, pAfCfg, sizeof(AfConfig)))
    {
        // todo ...
        memcpy(afcfg, pAfCfg, sizeof(AfConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_dzoom_set(DZoomConfig *pDZoomCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    DZoomConfig *dzoomcfg = &systemcfg->ptzCfg.dzoomCfg;

    if (memcmp(dzoomcfg, pDZoomCfg, sizeof(DZoomConfig)))
    {
        // todo ...
        memcpy(dzoomcfg, pDZoomCfg, sizeof(DZoomConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_ptz_scan_set(PTZScanConfig *pPtzScanCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    PTZScanConfig *scancfg = &systemcfg->ptzCfg.scanConfig;

    if (memcmp(scancfg, pPtzScanCfg, sizeof(PTZScanConfig)))
    {
        // todo ...
        memcpy(scancfg, pPtzScanCfg, sizeof(PTZScanConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_time_set(TimeConfig *pTimeCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    TimeConfig *timecfg = &systemcfg->timeCfg;

    if (memcmp(timecfg, pTimeCfg, sizeof(TimeConfig)))
    {
        if (pTimeCfg->timeZone != timecfg->timeZone)
        {
            __INFO("timezone:%d change to:%d\n", timecfg->timeZone, pTimeCfg->timeZone);
            anj_systime_set_zone_ex(pTimeCfg->timeZone, &pTimeCfg->summerConfig);
        }

        if (strcmp(pTimeCfg->timeMode.modeName, TIME_MODE_NAME_NTP) == 0)
        {
            anj_systime_ntp_update((const char *)pTimeCfg->ntpConfig.serverIP, 
                                pTimeCfg->ntpConfig.serverPort, 
                                pTimeCfg->ntpConfig.refreshInterval, 
                                pTimeCfg->timeZone);
        }
        else
        {
            anj_systime_ntp_stop();
        }

        memcpy(timecfg, pTimeCfg, sizeof(TimeConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_user_set(UserConfig *pUserCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    UserConfig *usercfg = &systemcfg->userCfg;

    if (memcmp(usercfg, pUserCfg, sizeof(UserConfig)))
    {
        // todo ...
        memcpy(usercfg, pUserCfg, sizeof(UserConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_syslog_set(SyslogConfig *pSyslogCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    SyslogConfig *syslogcfg = &systemcfg->syslogCfg;

    if (memcmp(syslogcfg, pSyslogCfg, sizeof(SyslogConfig)))
    {
        // todo ...
        memcpy(syslogcfg, pSyslogCfg, sizeof(SyslogConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_misc_set(MiscConfig *pMiscCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    MiscConfig *miscfg = &systemcfg->miscCfg;

    if (memcmp(miscfg, pMiscCfg, sizeof(MiscConfig)))
    {
        // todo ...
        memcpy(miscfg, pMiscCfg, sizeof(MiscConfig));
        anj_config_system_save(systemcfg);
        anj_osd_update_config();
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_maintain_set(MaintainConfig *pMaintianCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    MaintainConfig *maintaincfg = &systemcfg->maintainCfg;

    if (memcmp(maintaincfg, pMaintianCfg, sizeof(MaintainConfig)))
    {
        // todo ...
        memcpy(maintaincfg, pMaintianCfg, sizeof(MaintainConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_allowip_set(SysAlowIpConfig *pAllowIpCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    SysAlowIpConfig *allowipcfg = &systemcfg->alowipCfg;

    if (memcmp(allowipcfg, pAllowIpCfg, sizeof(SysAlowIpConfig)))
    {
        // todo ...
        memcpy(allowipcfg, pAllowIpCfg, sizeof(SysAlowIpConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_alarmclock_set(AlarmClockConfig *pAlarmClockCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    AlarmClockConfig *alarmclockcfg = &systemcfg->clockSetting;

    if (memcmp(alarmclockcfg, pAlarmClockCfg, sizeof(AlarmClockConfig)))
    {
        // todo ...
        memcpy(alarmclockcfg, pAlarmClockCfg, sizeof(AlarmClockConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_videoq_set(VideoQoSConfig *pVideoqCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    VideoQoSConfig *videoqcfg = &systemcfg->videoQosCfg;

    if (memcmp(videoqcfg, pVideoqCfg, sizeof(VideoQoSConfig)))
    {
        memcpy(videoqcfg, pVideoqCfg, sizeof(VideoQoSConfig));
        anj_config_system_save(systemcfg);
        anj_video_qos_notify();
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_audioprompt_set(AudioPromptConfig *pAudioPromptCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    AudioPromptConfig *audioprocfg = &systemcfg->audioPromptCfg;

    if (memcmp(audioprocfg, pAudioPromptCfg, sizeof(AudioPromptConfig)))
    {
        // todo ...
        memcpy(audioprocfg, pAudioPromptCfg, sizeof(AudioPromptConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_tamperproof_set(TamperProofConfig *pTamperProofCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    TamperProofConfig *tampercfg = &systemcfg->tamperProofCfg;

    if (memcmp(tampercfg, pTamperProofCfg, sizeof(TamperProofConfig)))
    {
        // todo ...
        memcpy(tampercfg, pTamperProofCfg, sizeof(TamperProofConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_location_set(LocationConfig *pLocationCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    LocationConfig *locationcfg = &systemcfg->locationCfg;

    if (memcmp(locationcfg, pLocationCfg, sizeof(LocationConfig)))
    {
        memcpy(locationcfg, pLocationCfg, sizeof(LocationConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_streetlamp_set(StreetLampConfig *pStreetLampCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    SystemConfig *systemcfg = (SystemConfig *)getSystemConfig();
    StreetLampConfig *streetlampcfg = &systemcfg->streetLampCfg;

    if (memcmp(streetlampcfg, pStreetLampCfg, sizeof(StreetLampConfig)))
    {
        memcpy(streetlampcfg, pStreetLampCfg, sizeof(StreetLampConfig));
        anj_config_system_save(systemcfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_system_alarmclock_get_by_xml(AlarmClockConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AlarmClockConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_alarmclock_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_audioprompt_get_by_xml(AudioPromptConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AudioPromptConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_audioprompt_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_syslog_get_by_xml(SyslogConfig *pSyslogCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "SystemLogConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_syslog_get(pNodelist->nodeItem, pSyslogCfg);
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

int anj_config_system_maintain_get_by_xml(MaintainConfig *pMaintainCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MaintainConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_maintain_get(pNodelist->nodeItem, pMaintainCfg);
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

int anj_config_system_allowip_get_by_xml(SysAlowIpConfig *pAllowIpCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AlowIpConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_allowip_get(pNodelist->nodeItem, pAllowIpCfg);
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

int anj_config_system_misc_get_by_xml(MiscConfig *pMiscCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MiscConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_misc_get(pNodelist->nodeItem, pMiscCfg);
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

int anj_config_system_ptz_get_by_xml(PTZConfig *pPtzCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "PTZConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_get(pNodelist->nodeItem, pPtzCfg);
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

int anj_config_system_tamperproof_get_by_xml(TamperProofConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "TamperProofConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_tamperproof_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_location_get_by_xml(LocationConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "LocationConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_location_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_streetlamp_get_by_xml(StreetLampConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "StreetLamp");
    if (pNodelist != NULL)
    {
        anj_config_system_streetlamp_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_time_get_by_xml(TimeConfig *pTimeCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "TimeConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_time_get(pNodelist->nodeItem, pTimeCfg);
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

int anj_config_system_user_get_by_xml(UserConfig *pUserCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "UserConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_user_get(pNodelist->nodeItem, pUserCfg);
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

int anj_config_system_videoq_get_by_xml(VideoQoSConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoQoSConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_videoq_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_get_by_xml(SystemConfig *pSystemCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "SystemConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_get(pNodelist->nodeItem, pSystemCfg);
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

int anj_config_system_ptz_advance_get_by_xml(PTZAdvanceConfig *pPtzAdvanceCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AdvanceConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_advance_get(pNodelist->nodeItem, pPtzAdvanceCfg);
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

int anj_config_system_ptz_dzoom_get_by_xml(DZoomConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DzoomConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_dzoom_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_ptz_common_get_by_xml(PTZCommonConfig *pPtzCommonCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "PTZConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_common_get(pNodelist->nodeItem, pPtzCommonCfg);
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

int anj_config_system_ptz_af_get_by_xml(AfConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AfConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_af_get(pNodelist->nodeItem, pCfg);
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

int anj_config_system_ptz_scan_get_by_xml(PTZScanConfig *pCfg, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "ScanConfig");
    if (pNodelist != NULL)
    {
        anj_config_system_ptz_scan_get(pNodelist->nodeItem, pCfg);
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
