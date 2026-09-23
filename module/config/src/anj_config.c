#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "anj_mw_comm.h"
#include "anj_mw_str.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_net.h"
#include "anj_comm.h"
#include "anj_config.h"
#include "anj_module.h"
#include "anj_sysmng.h"
#include "util_font.h"
#include "anj_factory.h"

#define STR_MAX_LEN 64

#define FILE_XML_ROOT_NAME1 "IPCConfig"
#define FILE_XML_ROOT_NAME2 "TOPSEEConfig"

static GlobalConfig gstConfig;
static GlobalConfig gstDefConfig;

static char s_default_config_file[128] = {0};    // 供onvif和http响应

char *anj_config_default_file_get()
{
    return s_default_config_file;    
}

static int anj_config_save(char *pCfgXml, const char *saveName)
{
    int iRet = 0;
    // 厂测模式，不保存任何配置
    DevInfo *pstDevInfo = getDevInfo();
    if (pstDevInfo->bFactoryMode)
    {
        return 0;
    }

    GlobalConfig cfg = {0};
    iRet = anj_config_parse(pCfgXml, &cfg);
    if (iRet == -1)
    {
        __ERR("parse fail\n");
        return -1;
    }
    else
    {
        __INFO("parse success\n");
    }

    int fd = open(saveName, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (fd == -1)
    {
        __ERR("open %s error\r\n", saveName);
        return -1;
    }

    safe_write(fd, pCfgXml, strlen(pCfgXml));
    fdatasync(fd);
    close(fd);

    return 0;
}

static char *anj_config_convert_xml(GlobalConfig *pCfg)
{
    char *pBuf = NULL;
    int initSize = 200;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, 0, initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<?xml version=\"1.0\" encoding=\"GB2312\" ?>\r\n");
    pb += snprintf(pb, pe - pb, "<");
    pb += snprintf(pb, pe - pb, FILE_XML_ROOT_NAME1);
    pb += snprintf(pb, pe - pb, ">\r\n");

    // todo
    // curPos = pb - pBuf;
    // tmp = MakeVersionCfgXml(&pCfg->versionCfg);
    // incrSize = strlen(tmp);
    // pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    // memcpy(pBuf + curPos, tmp, strlen(tmp));
    // curSize = curSize + incrSize;
    // pb = pBuf + curPos + strlen(tmp);
    // pe = pBuf + curSize - 1;

    // anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_system_conver_xml(&pCfg->systemCfg);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_network_conver_xml(&pCfg->networkCfgNew);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    anj_mw_free(tmp);

    // todo
    // curPos = pb - pBuf;
    // tmp = anj_config_server_conver_xml(&pCfg->serverCfg);
    // incrSize = strlen(tmp);
    // pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    // memcpy(pBuf + curPos, tmp, strlen(tmp));
    // curSize = curSize + incrSize;
    // pb = pBuf + curPos + strlen(tmp);
    // pe = pBuf + curSize - 1;

    // anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_media_conver_xml(&pCfg->mediaCfg, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_stream_conver_xml(&pCfg->mediaStreamCfg);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    anj_mw_free(tmp);

    // todo
    // curPos = pb - pBuf;
    // tmp = MakePlatformCfgXml(&pCfg->platformCfg, 1);
    // incrSize = strlen(tmp);
    // pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    // memcpy(pBuf + curPos, tmp, strlen(tmp));
    // curSize = curSize + incrSize;
    // pb = pBuf + curPos + strlen(tmp);
    // pe = pBuf + curSize - 1;

    // anj_mw_free(tmp);

    // todo
    // curPos = pb - pBuf;
    // tmp = MakeGB28181CfgXml(&pCfg->gb28181Cfg);
    // incrSize = strlen(tmp);
    // pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    // memcpy(pBuf + curPos, tmp, strlen(tmp));
    // curSize = curSize + incrSize;
    // pb = pBuf + curPos + strlen(tmp);
    // pe = pBuf + curSize - 1;

    // anj_mw_free(tmp);

    // curPos = pb - pBuf;
    // tmp = MakeGAT1400CfgXml(&pCfg->gat1400Cfg);
    // incrSize = strlen(tmp);
    // pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    // memcpy(pBuf + curPos, tmp, strlen(tmp));
    // curSize = curSize + incrSize;
    // pb = pBuf + curPos + strlen(tmp);
    // pe = pBuf + curSize - 1;

    // anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_record_conver_xml(pCfg->recordCfg, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_conver_xml(&pCfg->alarmCfg);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;

    // todo
    // if (pCfg->extChannelCfg.channelCnt > 0)
    // {
    //     curPos = pb - pBuf;
    //     tmp = MakeExtChannelConfigXml(&pCfg->extChannelCfg);
    //     incrSize = strlen(tmp);
    //     pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize + 1);
    //     memcpy(pBuf + curPos, tmp, strlen(tmp));
    //     curSize = curSize + incrSize;
    //     pb = pBuf + curPos + strlen(tmp);
    //     pe = pBuf + curSize - 1;
    // }

    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</");
    pb += snprintf(pb, pe - pb, FILE_XML_ROOT_NAME1);
    pb += snprintf(pb, pe - pb, ">\r\n");

    if (pb >= pBuf && (pb - pBuf) < curSize)
        *pb = '\0';
    else
        pBuf[curSize - 1] = '\0';

    return pBuf;
}

static int anj_config_all_get(IXML_Node *pNode, GlobalConfig *pCfg)
{
    memset(pCfg, 0, sizeof(GlobalConfig));

    IXML_Node *pChildNode = pNode->firstChild;

    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "RecordConfig"))
        {
            anj_config_record_get(pChildNode, pCfg->recordCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "ServerConfig"))
        {
            anj_config_server_get(pChildNode, &pCfg->serverCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "SystemConfig"))
        {
            anj_config_system_get(pChildNode, &pCfg->systemCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "NetworkConfig"))
        {
            anj_config_network_default(&pCfg->networkCfgNew);
            anj_config_network_get(pChildNode, &pCfg->networkCfgNew);
        }
        else if (!strcmp(pChildNode->nodeName, "MediaConfig"))
        {
            anj_config_media_default(&pCfg->mediaCfg);
            anj_config_media_get(pChildNode, &pCfg->mediaCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "MediaStreamConfig"))
        {
            anj_config_stream_default(&pCfg->mediaStreamCfg);
            anj_config_stream_get(pChildNode, &pCfg->mediaStreamCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "AlarmConfig"))
        {
            anj_config_alarm_get(pChildNode, &pCfg->alarmCfg, -1, 0);
        }
        else if (!strcmp(pChildNode->nodeName, "PlatformConfig"))
        {
            anj_config_platform_get(pChildNode, &pCfg->platformCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "GB28181Config"))
        {
            anj_config_gb28181_get(pChildNode, &pCfg->gb28181Cfg);
        }
        else if (!strcmp(pChildNode->nodeName, "GAT1400Config"))
        {
            anj_config_gat1400_get(pChildNode, &pCfg->gat1400Cfg);
        }
        else if (!strcmp(pChildNode->nodeName, "ConfigVersion"))
        {
            anj_config_version_get(pChildNode, &pCfg->versionCfg);
        }

        pChildNode = pChildNode->nextSibling;
    }

    return 0;
}

char *anj_config_devinfo_conver_xml()
{
    int maxSize = 1000;

    char *pe;
    char *pb;
    char *buf;

    DevInfo *pstDevInfo = getDevInfo();
    __ERR("Get device SN:%s, UUID:%s\n", pstDevInfo->sn, pstDevInfo->uuid);

    buf = (char *)anj_mw_malloc(maxSize);
    pb = buf;
    pe = buf + maxSize - 1;

    pb += snprintf(pb, pe - pb, "<DeviceInfo\r\n");
    pb += snprintf(pb, pe - pb, "SN=\"%s\"\r\n", pstDevInfo->sn);
    pb += snprintf(pb, pe - pb, "UUID=\"%s\"\r\n", pstDevInfo->uuid);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return buf;
}

char *anj_config_daytime_conver_xml(WorkDayTime *pWorkDayTime)
{
    int cntPerTime = 60;
    int totalSize = pWorkDayTime->timeSpancnt * cntPerTime + 120;

    char *pBuf = (char *)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<Workday\r\n");
    pb += snprintf(pb, pe - pb, "Day=\"%d\"\r\n", pWorkDayTime->workday);
    pb += snprintf(pb, pe - pb, ">\r\n");

    char buf[12];
    for (int i = 0; i < pWorkDayTime->timeSpancnt; i++)
    {
        pb += snprintf(pb, pe - pb, "<TimeSpan\r\n");
        GetDayTimeStr(buf, 12, &(pWorkDayTime->timeSpans[i].startTime));
        pb += snprintf(pb, pe - pb, "StartTime=\"%s\"\r\n", buf);
        GetDayTimeStr(buf, 12, &(pWorkDayTime->timeSpans[i].endTime));
        pb += snprintf(pb, pe - pb, "EndTime=\"%s\"\r\n", buf);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</Workday>\r\n");

    return pBuf;
}

char *anj_config_timespan_conver_xml(TimeSpanCfg *pTimeSpan)
{
    int initSize = 200;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;

    pb += snprintf(pb, pe - pb, "<TimeSpanCfg ");

    for (int i = 0; i < MAX_WORDDAYTIME_COUNT; i++)
    {
        pb += snprintf(pb, pe - pb, " day%d=\"%u\" ", i, pTimeSpan->workday[i]);
    }

    pb += snprintf(pb, pe - pb, " />\r\n");

    return buf;
}

char *anj_config_timespan_list_conver_xml(TimeSpanList *pTimeSpanList)
{
    int i = 0;
    int initSize = 100;
    char *tmp = NULL;
    int incrSize = 0;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<EnableTimeList>\r\n");

    curPos = pb - buf;
    for (i = 0; i < pTimeSpanList->workdayCnt; i++)
    {
        curPos = pb - buf;
        tmp = anj_config_daytime_conver_xml(&(pTimeSpanList->workdayTimes[i]));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);
    }

    pb += snprintf(pb, pe - pb, "</EnableTimeList>\r\n");

    return buf;
}

int anj_config_daytimespan_get(IXML_Node *pNode, DayTimeSpan *pDayTimeSpan)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "StartTime"))
        {
            GetTime(tmpAttr->nodeValue, &pDayTimeSpan->startTime);
        }
        else if (!strcmp(tmpAttr->nodeName, "EndTime"))
        {
            GetTime(tmpAttr->nodeValue, &pDayTimeSpan->endTime);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

int anj_config_daytime_get(IXML_Node *pNode, WorkDayTime *pWorkDayTime)
{
    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Day"))
        {
            pWorkDayTime->workday = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    int curIdx = 0;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "TimeSpan"))
        {
            anj_config_daytimespan_get(tmpChild, &(pWorkDayTime->timeSpans[curIdx]));
            childCnt++;
            curIdx++;
        }
        tmpChild = tmpChild->nextSibling;
    }

    pWorkDayTime->timeSpancnt = childCnt;

    return 0;
}

int anj_config_timespan_get(IXML_Node *pNode, TimeSpanCfg *pTimeSpan)
{
    int iRet = 0;
    ANJ_CHK(pNode != NULL && pTimeSpan != NULL, -1, "input Invalid");

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "day0"))
        {
            pTimeSpan->workday[0] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day1"))
        {
            pTimeSpan->workday[1] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day2"))
        {
            pTimeSpan->workday[2] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day3"))
        {
            pTimeSpan->workday[3] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day4"))
        {
            pTimeSpan->workday[4] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day5"))
        {
            pTimeSpan->workday[5] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "day6"))
        {
            pTimeSpan->workday[6] = (unsigned int)Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    CheckTimeSpanValid(pTimeSpan);
endFunc:
    return iRet;
}

static int anj_config_point_get(IXML_Node *pNode, AJ_POINT_S *pPoint)
{

    IXML_Node *tmpAttr = NULL;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "x"))
        {
            pPoint->x = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "y"))
        {
            pPoint->y = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

void anj_config_polygon_check(Polygon *pData)
{
    if (pData->count == 0)
    {
        pData->count = 4;
        pData->points[0].x = 0;
        pData->points[0].y = 0;
        pData->points[1].x = 100;
        pData->points[1].y = 0;
        pData->points[2].x = 100;
        pData->points[2].y = 100;
        pData->points[3].x = 0;
        pData->points[3].y = 100;
    }

    int bAllSame = 1;
    int iIndex;
    for (iIndex = 1; iIndex < pData->count && iIndex < MAX_POLYGON_POINT_CNT; iIndex++)
    {
        if (pData->points[iIndex].x != pData->points[iIndex - 1].x || pData->points[iIndex].y != pData->points[iIndex - 1].y)
        {
            bAllSame = 0;
            break;
        }
    }

    if (bAllSame > 0)
    {
        pData->count = 4;
        pData->points[0].x = 0;
        pData->points[0].y = 0;
        pData->points[1].x = 100;
        pData->points[1].y = 0;
        pData->points[2].x = 100;
        pData->points[2].y = 100;
        pData->points[3].x = 0;
        pData->points[3].y = 100;
    }
}

int anj_config_polygon_get(IXML_Node *pNode, Polygon *pPolygon)
{
    IXML_Node *tmpChild = NULL;
    IXML_Node *tmpAttr = NULL;
    int childCnt = 0;
    int curIdx = 0;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PointCnt"))
        {
            pPolygon->count = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "Point"))
        {
            anj_config_point_get(tmpChild, &(pPolygon->points[curIdx]));
            childCnt++;
            curIdx++;
            if (childCnt >= MAX_POLYGON_POINT_CNT)
            {
                break;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    if (pPolygon->count != childCnt)
    {
        pPolygon->count = childCnt;
    }

    return 0;
}

char *anj_config_polygon_conver_xml(Polygon *pPolygon)
{
    int initSize = 1024;

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;

    pb += snprintf(pb, pe - pb, "<Polygon PointCnt=\"%d\">\r\n", pPolygon->count);

    int i = 0;

    for (i = 0; i < pPolygon->count && i < MAX_POLYGON_POINT_CNT; i++)
    {
        pb += snprintf(pb, pe - pb, "<Point x=\"%d\"  y=\"%d\"/>\r\n", pPolygon->points[i].x, pPolygon->points[i].y);
    }

    pb += snprintf(pb, pe - pb, "</Polygon>\r\n");

    return buf;
}
int anj_config_timespan_list_get(IXML_Node *pNode, TimeSpanList *pTimeSpanList)
{
    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    int curIdx = 0;

    memset(pTimeSpanList, 0, sizeof(TimeSpanList));

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "Workday"))
        {
            anj_config_daytime_get(tmpChild, &(pTimeSpanList->workdayTimes[curIdx]));
            childCnt++;
            curIdx++;
        }

        tmpChild = tmpChild->nextSibling;
    }

    pTimeSpanList->workdayCnt = childCnt;

    //	ShowSchedule(pTimeSpanList);

    if (pTimeSpanList->workdayCnt == 0)
    {
        pTimeSpanList->workdayCnt = 1;
        pTimeSpanList->workdayTimes[0].workday = 7;
        pTimeSpanList->workdayTimes[0].timeSpancnt = 1;
        pTimeSpanList->workdayTimes[0].timeSpans[0].endTime.hour = 23;
        pTimeSpanList->workdayTimes[0].timeSpans[0].endTime.minute = 59;
        pTimeSpanList->workdayTimes[0].timeSpans[0].endTime.sec = 59;
    }

    return 0;
}

int anj_config_zoom_exist(const char *filePath)
{
    int ret = 0;
    char *pCfgXml = anj_mw_read_file_buffer(filePath);
    if (pCfgXml == NULL)
    {
        return ret;
    }

    if (strstr(pCfgXml, "<DzoomConfig") != NULL &&
        strstr(pCfgXml, "multiple_max") != NULL &&
        strstr(pCfgXml, "multiple_set") != NULL)
    {
        ret = 1;
    }
    else
    {
        ret = 0;
    }

    anj_mw_free(pCfgXml);
    return ret;
}

double anj_config_zoom_multile_parse(const char *pFile)
{
    if (NULL == pFile)
        return -1.0;

    char *pCfgXml = anj_mw_read_file_buffer(pFile);
    if (pCfgXml == NULL)
    {
        return -1.0;
    }

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);

    if (pDocNode == NULL)
    {
        __ERR("ixmlParseBuffer error\r\n");
        anj_mw_free(pCfgXml);
        return -1.0;
    }

    double data = -1.0;

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "DZOOMSETTING");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpChild = pNode->firstChild;
        IXML_Node *tmpAttr = NULL;
        while (tmpChild != NULL)
        {
            if (strcmp(tmpChild->nodeName, "SETTING") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "MULTIPLE") == 0)
                    {
                        if (tmpAttr->nodeValue != NULL)
                            data = atof(tmpAttr->nodeValue);
                    }

                    tmpAttr = tmpAttr->nextSibling;
                }
            }

            tmpChild = tmpChild->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
    }

    __ERR("Got multiple=%f from %s", data, pFile);

    ixmlDocument_free(pDocNode);
    anj_mw_free(pCfgXml);

    return data;
}

double anj_config_zoom_multile_get_by_xml()
{
    double data = -1.0;
    char szOemXMLFileName[128] = {0};
    snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", DATA_BLOCK_MOUNT_PATH, DZOOM_CUST_SETTING_FILE);
    data = anj_config_zoom_multile_parse(szOemXMLFileName);
    if (data < 0)
    {
        snprintf(szOemXMLFileName, sizeof(szOemXMLFileName), "%s/%s", AJ_APP_PATH, DZOOM_CUST_SETTING_FILE);
        data = anj_config_zoom_multile_parse(szOemXMLFileName);
    }

    return data;
}

void anj_config_zoom_multile_save(double multiple)
{
    const char *szFile = DZOOM_CUST_SETTING_FILE_FULL;
    char buffer[256] = {0};
    snprintf(buffer, sizeof(buffer), "<DZOOMSETTING><SETTING MULTIPLE=\"%f\"/></DZOOMSETTING>", multiple);
    write_buffer_to_file(szFile, buffer, strlen(buffer));
}

char *anj_config_pos_value_get(IXML_Document *pDoc, char *fieldName)
{
    char *fieldValue = NULL;

    const char *tagName = "POS";
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDoc, tagName);
    if (pNodelist == NULL)
    {
        __ERR("POS not found from message body.\n");
        return NULL;
    }
    else
    {
        IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
        while (tmp != NULL)
        {
            if (strcmp(tmp->nodeName, fieldName) == 0)
            {
                if (tmp->nodeValue != NULL)
                {
                    fieldValue = anj_mw_malloc(strlen(tmp->nodeValue) + 1);
                    strcpy(fieldValue, tmp->nodeValue);

                    ixmlNodeList_free(pNodelist);
                    return fieldValue;
                }
            }

            tmp = tmp->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        return NULL;
    }
}

int anj_config_load_cust()
{
    int iRet = 0;
    char *pCfgXml = NULL;
    char cmd[256] = {0};
    char szCustFile[64] = {0};
    snprintf(szCustFile, sizeof(szCustFile), "%s/%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME, AJ_XML_CUR_CONFIG_FILE_NAME);

    pCfgXml = anj_mw_read_file_buffer(szCustFile);
    ANJ_CHK((pCfgXml != NULL), -1, "can not read oem specific config file");

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK((pDocNode != NULL), -1, "ixmlParseBuffer error");

    /*
    <?xml version="1.0" encoding="gb2312" ?>
    <CUR_CONFIG>
    <CLEAR_ALL_CUST DATA="1" />
    <MAC DATA="00:35:c7:e3:7e:ac" />
    <LANGUAGE DATA="zh_cn" />
    <IPADDR DATA1="192.168.0.200" DATA2="255.255.255.0" DATA3="192.168.0.1" />
    <PASSWORD DATA="123456" />
    <FRENQUENCE DATA="1" />
    <BRIGHT DATA="128" />
    <CONTRANST DATA="128" />
    <SHARP DATA="128" />
    <SATURATION DATA="64" />
    </CUR_CONFIG>

    */

    char szValue[STR_MAX_LEN] = {0};
    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "CUR_CONFIG");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpChild = pNode->firstChild;
        IXML_Node *tmpAttr = NULL;
        while (tmpChild != NULL)
        {
            if (strcmp(tmpChild->nodeName, "CLEAR_ALL_CUST") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "DATA") == 0)
                    {
                        int nData = atoi(tmpAttr->nodeValue);

                        if (nData > 0)
                        {
                            memset(cmd, 0, sizeof(cmd));
                            snprintf(cmd, sizeof(cmd), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
                            remove(cmd);

                            memset(cmd, 0, sizeof(cmd));
                            snprintf(cmd, sizeof(cmd), "%s/%s", DATA_BLOCK_MOUNT_PATH, FAKE_VIDEO_RES_FILE);
                            remove(cmd);

                            memset(cmd, 0, sizeof(cmd));
                            snprintf(cmd, sizeof(cmd), "%s/%s", DATA_BLOCK_MOUNT_PATH, "factory.default.cfg");
                            remove(cmd);
                        }
                        break;
                    }
                    tmpAttr = tmpAttr->nextSibling;
                }
            }

            if (strcmp(tmpChild->nodeName, "NETWORK") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "MAC") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);

                        if (is_mac_addr_valid(szValue))
                        {
                            strcpy((char *)gstConfig.networkCfgNew.lanCfg.MACAddress, szValue);
                        }
                    }

                    if (strcmp(tmpAttr->nodeName, "IPADDR") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);
                        strcpy((char *)gstConfig.networkCfgNew.lanCfg.IPAddress, szValue);
                    }
                    if (strcmp(tmpAttr->nodeName, "MASK") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);
                        strcpy((char *)gstConfig.networkCfgNew.lanCfg.netMask, szValue);
                    }
                    if (strcmp(tmpAttr->nodeName, "GATEWAY") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);
                        strcpy((char *)gstConfig.networkCfgNew.lanCfg.gateWay, szValue);
                    }
                    tmpAttr = tmpAttr->nextSibling;
                }
                anj_config_network_save(&gstConfig.networkCfgNew);
            }

            if (strcmp(tmpChild->nodeName, "SYSTEM") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                while (tmpAttr)
                {
                    if (strcmp(tmpAttr->nodeName, "LANGUAGE") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);
                        strcpy((char *)gstConfig.systemCfg.miscCfg.language, szValue);
                    }
                    if (strcmp(tmpAttr->nodeName, "PASSWORD") == 0)
                    {
                        strncpy(szValue, tmpAttr->nodeValue, STR_MAX_LEN - 1);
                        strcpy((char *)gstConfig.systemCfg.userCfg.accounts[0].password, szValue);
                    }
                    tmpAttr = tmpAttr->nextSibling;
                }
                anj_config_system_save(&gstConfig.systemCfg);
            }

            if (strcmp(tmpChild->nodeName, "VIDEO") == 0)
            {
                tmpAttr = tmpChild->firstAttr;
                for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
                {
                    VideoConfig *pstVideoConfig = &gstConfig.mediaCfg.videoConfig[cameraIndex];
                    while (tmpAttr)
                    {
                        if (strcmp(tmpAttr->nodeName, "FRENQUENCE") == 0)
                        {
                            pstVideoConfig->videoCapture.tvsystem = atoi(tmpAttr->nodeValue);
                        }
                        if (strcmp(tmpAttr->nodeName, "BRIGHT") == 0)
                        {
                            pstVideoConfig->videoCapture.brightness = atoi(tmpAttr->nodeValue);
                        }
                        if (strcmp(tmpAttr->nodeName, "CONTRANST") == 0)
                        {
                            pstVideoConfig->videoCapture.contrast = atoi(tmpAttr->nodeValue);
                        }
                        if (strcmp(tmpAttr->nodeName, "SHARP") == 0)
                        {
                            pstVideoConfig->videoCapture.sharpness = atoi(tmpAttr->nodeValue);
                        }
                        if (strcmp(tmpAttr->nodeName, "SATURATION") == 0)
                        {
                            pstVideoConfig->videoCapture.saturation = atoi(tmpAttr->nodeValue);
                        }

                        // OSD
                        if (strcmp(tmpAttr->nodeName, "OSDENABLE") == 0)
                        {
                            pstVideoConfig->overlay.enable = atoi(tmpAttr->nodeValue);
                        }
                        if (strcmp(tmpAttr->nodeName, "OSDPOSITION") == 0)
                        {
                            int nPosData = atoi(tmpAttr->nodeValue);
                            pstVideoConfig->overlay.timeOverlay.posX = (nPosData >> 12) & 0xf;
                            pstVideoConfig->overlay.timeOverlay.posY = (nPosData >> 8) & 0xf;
                            pstVideoConfig->overlay.titleOverlay.posX = (nPosData >> 4) & 0xf;
                            pstVideoConfig->overlay.titleOverlay.posY = (nPosData >> 0) & 0xf;
                        }
                        if (strcmp(tmpAttr->nodeName, "OSDTITLE") == 0)
                        {
                            char szValue[TITLE_MAX_LEN];
                            strncpy(szValue, tmpAttr->nodeValue, TITLE_MAX_LEN - 1);

                            str_gb2312_2_utf8(szValue, pstVideoConfig->overlay.titleOverlay.title_utf8, 0);
                        }
                        if (strcmp(tmpAttr->nodeName, "OSDSTYLE") == 0)
                        {
                            int nPosData = atoi(tmpAttr->nodeValue);
                            pstVideoConfig->overlay.style = nPosData;
                        }

                        tmpAttr = tmpAttr->nextSibling;
                    }
                }
                anj_config_media_save(&gstConfig.mediaCfg);
            }

            tmpChild = tmpChild->nextSibling;
        }
        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(NetworkConfig) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

endFunc:
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    return iRet;
}

int anj_config_load(const char *pCfgName, void *pConfig, char *filePath)
{
    int iRet = 0;
    char *pCfgXml = NULL;

    ANJ_CHK((pConfig != NULL) && (filePath != NULL), -1, "input Invalid");
    pCfgXml = anj_mw_read_file_buffer(filePath);
    ANJ_CHK(pCfgXml != NULL, -1, "read failed");
    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    IXML_NodeList *pNodelist = NULL;
    if (NULL == pCfgName)
    {
        pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME1);
        if (NULL == pNodelist)
        {
            pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME2);
        }
    }
    else
    {
        char tmp[64];
        sprintf(tmp, "%s", pCfgName);
        pNodelist = ixmlDocument_getElementsByTagName(pDocNode, tmp);
    }

    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;

        if (NULL == pCfgName)
        {
            iRet = anj_config_all_get(pNode, (GlobalConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "MediaConfig"))
        {
            iRet = anj_config_media_get(pNode, (MediaConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "AlarmConfig"))
        {
            iRet = anj_config_alarm_get(pNode, (AlarmConfig *)pConfig, -1, 0);
        }
        else if (!strcmp(pCfgName, "MediaStreamConfig"))
        {
            iRet = anj_config_stream_get(pNode, (MediaStreamConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "NetworkConfig"))
        {
            iRet = anj_config_network_get(pNode, (NetworkConfigNew *)pConfig);
        }
        else if (!strcmp(pCfgName, "ServerConfig"))
        {
            iRet = anj_config_server_get(pNode, (ServerConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "SystemConfig"))
        {
            iRet = anj_config_system_get(pNode, (SystemConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "RecordConfig"))
        {
            iRet = anj_config_record_get(pNode, (RecordConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "PlatformConfig"))
        {
            iRet = anj_config_platform_get(pNode, (PlatformConfig *)pConfig);
        }
        else if (!strcmp(pCfgName, "GB28181Config"))
        {
            iRet = anj_config_gb28181_get(pNode, (GB28181Config *)pConfig);
        }
        else if (!strcmp(pCfgName, "GAT1400Config"))
        {
            iRet = anj_config_gat1400_get(pNode, (GAT1400Config *)pConfig);
        }
        else if (!strcmp(pCfgName, "ConfigVersion"))
        {
            iRet = anj_config_version_get(pNode, (ConfigVersion *)pConfig);
        }

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(NetworkConfig) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

endFunc:
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    return iRet;
}

int anj_config_save_node(char *pNodeXml, const char *pNodeIdStart, const char *pNodeIdEnd)
{
    int iRet = 0;
    int pos = 0;
    int bufSize = 0;

    char *pe = NULL;
    char *pb = NULL;
    char *buf = NULL;
    char *pCfgBuf = NULL;

    ANJ_CHK((pNodeXml != NULL) && (pNodeIdStart != NULL) && (pNodeIdEnd != NULL), -1, "input Invalid");

    pCfgBuf = anj_mw_read_file_buffer(CONFIG_FILE_PATH);
    ANJ_CHK(pCfgBuf != NULL, -1, "read failed");

    int cfgLen = strlen(pCfgBuf);
    int nNodeLen = strlen(pNodeXml);

    do
    {
        pb = strstr(pCfgBuf, pNodeIdStart);
        if (pb != NULL)
        {
            pe = strstr(pb, pNodeIdEnd) + strlen(pNodeIdEnd);
        }
        else
        {
            char szIdentity[64];
            sprintf(szIdentity, "</%s>", FILE_XML_ROOT_NAME1);
            pb = strstr(pCfgBuf, szIdentity);
            if (pb == NULL)
            {
                sprintf(szIdentity, "</%s>", FILE_XML_ROOT_NAME2);
                pb = strstr(pCfgBuf, szIdentity);
            }
            if (pb != NULL)
            {
                pe = pb;
            }
            else
            {
                goto endFunc;
            }
        }

        while (!isprint(*pe) && pe < pCfgBuf + cfgLen)
        {
            pe++;
        }

        bufSize = cfgLen - (pe - pb) + nNodeLen + 1;
        buf = (char *)anj_mw_malloc(bufSize);
        if (NULL == buf)
        {
            goto endFunc;
        }
        memset(buf, '\0', bufSize);

        memcpy(buf + pos, pCfgBuf, pb - pCfgBuf);
        pos += (pb - pCfgBuf);

        memcpy(buf + pos, pNodeXml, nNodeLen);
        pos += nNodeLen;

        size_t remainingLen = cfgLen - (pe - pCfgBuf);
        memcpy(buf + pos, pe, cfgLen - (pe - pCfgBuf));
        pos += remainingLen;
        buf[pos] = '\0';
    } while (0);

endFunc:
    if (pCfgBuf)
    {
        anj_mw_free(pCfgBuf);
    }

    if (NULL != buf)
    {
        iRet = anj_config_save(buf, CONFIG_FILE_PATH);
        if (iRet < 0)
        {
            __ERR("can not save %s cfg\n", pNodeIdStart);
        }

        anj_mw_free(buf);
    }

    return iRet;
}

int anj_config_save_all(GlobalConfig *pCfg, const char *filename)
{
    int iRet = 0;
    char *pBuf = NULL;
    ANJ_CHK((pCfg != NULL) && (filename != NULL), -1, "input Invalid");

    pBuf = anj_config_convert_xml(pCfg);
    ANJ_CHK((pBuf != NULL), -1, "convert xml failed");
    ANJ_CHK_FUNC(anj_config_save(pBuf, filename), 0, "config save failed");

endFunc:
    if (pBuf)
    {
        anj_mw_free(pBuf);
    }
    return iRet;
}

int anj_config_parse_user_cmd(char *xmlBuf)
{
    int iRet = 0;
    IXML_Document *pDocNode = NULL;
    IXML_NodeList *pNodelist = NULL;
    ANJ_CHK((xmlBuf != NULL), -1, "input Invalid");

    pDocNode = ixmlParseBuffer(xmlBuf);
    ANJ_CHK((pDocNode != NULL), -1, "xml Invalid");

    pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "EXECUTE_USER_CMD");
    ANJ_CHK((pNodelist != NULL), -1, "EXECUTE_USER_CMD Invalid");

    int found = 0;
    IXML_Node *tmpChild = pNodelist->nodeItem->firstChild;
    while (tmpChild != NULL)
    {
        if (strcmp(tmpChild->nodeName, "CMD") == 0)
        {
            IXML_Node *tmp = tmpChild->firstAttr;
            while (tmp != NULL)
            {
                if (strcmp(tmp->nodeName, "DATA") == 0)
                {
                    if (tmp->nodeValue != NULL)
                    {
                        char cmd[512] = {0};
                        strncpy(cmd, tmp->nodeValue, sizeof(cmd) - 1);
                        __ERR("%s\n", cmd);
                        anj_mw_system(cmd);

                        found = 1;
                    }
                }

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }
    if (found > 0)
    {
        iRet = 0;
    }
    else
    {
        __ERR("CMD is empty\n");
        iRet = -1;
    }
endFunc:
    if (pNodelist)
    {
        ixmlNodeList_free(pNodelist);
    }
    if (pDocNode)
    {
        ixmlDocument_free(pDocNode);
    }
    return iRet;
}

int anj_config_parse_file(const char *pFileName, GlobalConfig *pCfg)
{
    int iRet = 0;
    char *pCfgXml = NULL;

    ANJ_CHK((pFileName != NULL), -1, "input Invalid");

    pCfgXml = anj_mw_read_file_buffer(pFileName);
    ANJ_CHK(pCfgXml != NULL, -1, "read failed");

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME1);
    if (NULL == pNodelist)
    {
        pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME2);
    }

    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        iRet = anj_config_all_get(pNode, (GlobalConfig *)pCfg);

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(NetworkConfig) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

endFunc:
    if (pCfgXml)
    {
        anj_mw_free(pCfgXml);
    }
    return iRet;
}

int anj_config_parse(const char *pCfgXml, GlobalConfig *pCfg)
{
    int iRet = 0;
    ANJ_CHK((pCfgXml != NULL), -1, "input Invalid");

    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    ANJ_CHK(pDocNode != NULL, -1, "ixmlParseBuffer error");

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME1);
    if (NULL == pNodelist)
    {
        pNodelist = ixmlDocument_getElementsByTagName(pDocNode, FILE_XML_ROOT_NAME2);
    }

    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        iRet = anj_config_all_get(pNode, pCfg);

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(NetworkConfig) return NULL!\n");
        ixmlDocument_free(pDocNode);
        iRet = -1;
    }

endFunc:
    return iRet;
}

int anj_config_get_default(char *pSrcFile, int iFilenameLen, GlobalConfig *pConfig)
{	
    char szHeadPath[32] = {0};
    char szDefaultConfigFileName[64] = {0};
    char szDefaultConfigFileName2[32] = {0};
    snprintf(szDefaultConfigFileName, sizeof(szDefaultConfigFileName), "config.default.%s.xml", ANJ_PROJECT_NAME);
    toLowerStr(szDefaultConfigFileName);
    snprintf(szDefaultConfigFileName2, sizeof(szDefaultConfigFileName2), "config.default.xml");

    //第一优先级
    snprintf(szHeadPath, sizeof(szHeadPath), "%s", DATA_BLOCK_MOUNT_PATH);

    pSrcFile[0] = 0;
    snprintf(pSrcFile, iFilenameLen, "%s/%s/%s",  szHeadPath, AJ_CUST_PATH_NAME, szDefaultConfigFileName);

    int bFound = 0;
    if (anj_mw_file_exists(pSrcFile))
    {
        if(anj_config_parse_file(pSrcFile, pConfig) == 0)
        {
            bFound = 1;
            __INFO("parse ok: %s\n", pSrcFile);
        }
        else
        {
            __ERR("parse failed: %s\n", pSrcFile);
        }
    }
    else
    {
        __DBG("Not file exist %s\n", pSrcFile);
    }

    if(bFound == 0)
    {
        pSrcFile[0] = 0;
        snprintf(pSrcFile, iFilenameLen, "%s/%s/%s", szHeadPath, AJ_CUST_PATH_NAME, szDefaultConfigFileName2);

        if (anj_mw_file_exists(pSrcFile))
        {
            if(anj_config_parse_file(pSrcFile, pConfig) == 0)
            {
                bFound = 1;
            }
            else
            {
                __ERR("src file:%s parse failed\n", pSrcFile);
            }
        }
        else
        {
            __DBG("src file:%s don't exist\n", pSrcFile);
        }
    }

    snprintf(szHeadPath, sizeof(szHeadPath), "%s", AJ_APP_PATH);

    if(bFound == 0)
    {
        pSrcFile[0] = 0;
        snprintf(pSrcFile, iFilenameLen, "%s/%s", szHeadPath, szDefaultConfigFileName);

        if (anj_mw_file_exists(pSrcFile))
        {
            if(anj_config_parse_file(pSrcFile, pConfig) == 0)
            {
                bFound = 1;
            }
            else
            {
                __ERR("src file:%s parse failed\n", pSrcFile);
            }
        }
        else
        {
            __DBG("src file:%s don't exist\n", pSrcFile);
        }
    }

    if( bFound == 0)
    {
        pSrcFile[0] = 0;
        snprintf(pSrcFile, iFilenameLen, "%s/%s", szHeadPath, szDefaultConfigFileName2);

        if (anj_mw_file_exists(pSrcFile))
        {
            if(anj_config_parse_file(pSrcFile, pConfig) == 0)
            {
                bFound = 1;
            }
            else
            {
                __ERR("src file:%s parse failed\n", pSrcFile);
            }
        }
        else
        {
            __DBG("src file:%s don't exist\n", pSrcFile);
        }
    }

    if(bFound > 0)
    {
        __INFO("product use default config:%s\n", pSrcFile);
        return 0;
    }

    __INFO("product cannot found default config\n");
    return -1;
}

int anj_config_cust_default_path(char *buf, int len)
{
    char szDir[64] = {0};
    char szName[64] = {0};

    if (buf == NULL || len <= 0)
    {
        return -1;
    }

    snprintf(szDir, sizeof(szDir), "%s/%s", DATA_BLOCK_MOUNT_PATH, AJ_CUST_PATH_NAME);
    mkdir(szDir, 0777);

    snprintf(szName, sizeof(szName), "config.default.%s.xml", ANJ_PROJECT_NAME);
    snprintf(buf, len, "%s/%s", szDir, toLowerStr(szName));
    return 0;
}

int anj_config_copy_default(void)
{
    char srcFile[128] = {0};

    if (anj_config_get_default(srcFile, sizeof(srcFile), &gstDefConfig) != 0)
    {
        return -1;
    }

    return anj_mw_file_copy(srcFile, CONFIG_FILE_PATH);
}

int anj_config_init()
{
    __LOG_ENTER();
    int bNewConfig = 0;

    memset(&gstConfig, 0, sizeof(GlobalConfig));
    memset(&gstDefConfig, 0, sizeof(GlobalConfig));

    pthread_rwlock_init(&gstConfig.rwlock, NULL);

    if (anj_mw_file_exists(CONFIG_FILE_PATH) == 0)
    {
        anj_config_copy_default();
        bNewConfig = 1;
    }

    if (anj_config_get_default(s_default_config_file, sizeof(s_default_config_file), &gstDefConfig) != 0)
    {
        char configDefaultPath[64] = {0};
        s_default_config_file[0] = 0;
        snprintf(configDefaultPath, sizeof(configDefaultPath), CONFIG_FILE_DEFAULT_PATH, ANJ_PROJECT_NAME);
        anj_config_load(NULL, &gstDefConfig, toLowerStr(configDefaultPath));
    }

    int iRet = anj_config_load(NULL, &gstConfig, CONFIG_FILE_PATH);
    __INFO("anj init config:%s\n", (iRet == 0) ? "success" : "failed");
    if (iRet != 0)
    {
        anj_config_copy_default();
        iRet = anj_config_load(NULL, &gstConfig, CONFIG_FILE_PATH);
        bNewConfig = 1;
    }

    if (bNewConfig && iRet == 0)
    {
        anj_sysmng_cust_language_restore(&gstConfig);
        anj_sysmng_factory_config_restore(&gstConfig);
        anj_config_save_all(&gstConfig, CONFIG_FILE_PATH);
    }

    if (iRet == 0)
    {
        if (anj_config_oem_second_gate_a())
        {
            anj_sysmng_second_config_apply(&gstConfig);
            anj_config_save_all(&gstConfig, CONFIG_FILE_PATH);
        }

        /* 默认配置为 DHCP 且开启 dhcpOnReboot 时，本次启动强制走 DHCP */
        if (gstDefConfig.networkCfgNew.lanCfg.dhcpEnable && gstConfig.systemCfg.tamperProofCfg.dhcpOnReboot)
        {
            gstConfig.networkCfgNew.lanCfg.dhcpEnable = 1;
        }
    }

    __LOG_LEAVE();
    return iRet;
}

int anj_config_uninit()
{
    pthread_rwlock_destroy(&gstConfig.rwlock);
    return 0;
}

void *getGlbConfig(void)
{
    return &gstConfig;
}

void *getMediaConfig(void)
{
    return &gstConfig.mediaCfg;
}

void *getMediaDefConfig(void)
{
    return &gstDefConfig.mediaCfg;
}

void *getMediaStreamConfig(void)
{
    return &gstConfig.mediaStreamCfg;
}

void *getRecordConfig(void)
{
    return gstConfig.recordCfg;
}

void *getNetWorkConfig(void)
{
    return &gstConfig.networkCfgNew;
}

void *getAlarmConfig(void)
{
    return &gstConfig.alarmCfg;
}

void *getSystemConfig(void)
{
    return &gstConfig.systemCfg;
}

void *getGb28181Config(void)
{
    return &gstConfig.gb28181Cfg;
}

void *getGat1400Config(void)
{
    return &gstConfig.gat1400Cfg;
}

void *getServerConfig(void)
{
    return &gstConfig.serverCfg;
}

void *getPlatformConfig(void)
{
    return &gstConfig.platformCfg;
}

void *getVersionConfig(void)
{
    return &gstConfig.versionCfg;
}

void *getRWlock(void)
{
    return &gstConfig.rwlock;
}

REGISTER_MODULE(anj_config, MODULE_PRIORITY_CONFIG);
