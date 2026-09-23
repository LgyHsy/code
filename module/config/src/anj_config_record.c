
#include <stdlib.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_config.h"
#include "anj_record.h"
#include "eventhub.h"

static int anj_config_record_comm_get(IXML_Node *pNode, RecordCommConfig *pCommCfg)
{
    memset(pCommCfg, 0, sizeof(RecordCommConfig));

    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "LocalEnable"))
        {
            pCommCfg->localEnable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RemoteEnable"))
        {
            pCommCfg->remoteEnable = (NetworkStorageType)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StoragePolicy"))
        {
            memset(pCommCfg->storePolicy.policyName, '\0', RECORD_STORAGE_POLICY_MAX_LEN);
            StrCpy(pCommCfg->storePolicy.policyName, RECORD_STORAGE_POLICY_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StorageSequence"))
        {
            memset(pCommCfg->storageSequence, '\0', MAX_STORAGE_SEQUENCE_NAME_LEN);
            StrCpy(pCommCfg->storageSequence, MAX_STORAGE_SEQUENCE_NAME_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "MountParam"))
        {
            memset(pCommCfg->mountParam, '\0', MAX_REMOTE_MOUNT_PARAM_LEN);
            StrCpy(pCommCfg->mountParam, MAX_REMOTE_MOUNT_PARAM_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RecordFileSize"))
        {
            pCommCfg->recordFileSize = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "recordFileKeeyDays"))
        {
            pCommCfg->recordFileKeeyDays = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "timelapseEnable"))
        {
            pCommCfg->timelapseCfg.timelapseEnable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "timelapseSec"))
        {
            pCommCfg->timelapseCfg.timelapseSec = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "timelapseFps"))
        {
            pCommCfg->timelapseCfg.timelapseFps = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "timelapseFileSize"))
        {
            pCommCfg->timelapseCfg.timelapseFileSize = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (pCommCfg->timelapseCfg.timelapseFileSize < 10)
        pCommCfg->timelapseCfg.timelapseFileSize = 10;
    else if (pCommCfg->timelapseCfg.timelapseFileSize > 2048)
        pCommCfg->timelapseCfg.timelapseFileSize = 2048;

    if (pCommCfg->timelapseCfg.timelapseSec < 1)
        pCommCfg->timelapseCfg.timelapseSec = 4;
    else if (pCommCfg->timelapseCfg.timelapseSec > 3600)
        pCommCfg->timelapseCfg.timelapseSec = 3600;

    if (pCommCfg->timelapseCfg.timelapseFps < 1)
        pCommCfg->timelapseCfg.timelapseFps = 25;
    else if (pCommCfg->timelapseCfg.timelapseFps > 60)
        pCommCfg->timelapseCfg.timelapseFps = 60;

    return 0;
}

static int anj_config_record_schedule_get(IXML_Node *pNode, ScheduleRecordConfig *pScheduleCfg)
{
    memset(pScheduleCfg, 0, sizeof(ScheduleRecordConfig));

    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Stream"))
        {
            pScheduleCfg->stream = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FileFormat"))
        {
            memset(pScheduleCfg->fileFormat.formatName, '\0', RECORD_FILEFORMAT_MAX_LEN);
            StrCpy(pScheduleCfg->fileFormat.formatName, RECORD_FILEFORMAT_MAX_LEN, "MP4");
        }
        else if (!strcmp(tmpAttr->nodeName, "MediaType"))
        {
            memset(pScheduleCfg->mediaType.typeName, '\0', RECORD_MEDIA_TYPE_MAX_LEN);
            StrCpy(pScheduleCfg->mediaType.typeName, RECORD_MEDIA_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LocalStore"))
        {
            pScheduleCfg->localStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RemoteStore"))
        {
            pScheduleCfg->remoteStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "JpegInterval"))
        {
            pScheduleCfg->jpgInterval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FtpUpload"))
        {
            pScheduleCfg->ftpUpload = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EmailUpload"))
        {
            pScheduleCfg->emailUpload = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "EnableTimeList"))
        {
            TimeSpanList timeSpanList;
            anj_config_timespan_list_get(tmpChild, &timeSpanList);
            TransTimeSpan2New(&timeSpanList, &pScheduleCfg->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pScheduleCfg->timeSpan);
        }
        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int anj_config_record_alarm_get(IXML_Node *pNode, AlarmRecordConfig *pAlarmRecordCfg)
{
    memset(pAlarmRecordCfg, 0, sizeof(AlarmRecordConfig));
    pAlarmRecordCfg->stream = 1;
    IXML_Node *tmpAttr = NULL;

    int bGetAlarmBits = 0;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Stream"))
        {
            pAlarmRecordCfg->stream = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FileFormat"))
        {
            memset(pAlarmRecordCfg->fileFormat.formatName, '\0', RECORD_FILEFORMAT_MAX_LEN);
            StrCpy(pAlarmRecordCfg->fileFormat.formatName, RECORD_FILEFORMAT_MAX_LEN, "MP4");
        }
        else if (!strcmp(tmpAttr->nodeName, "MediaType"))
        {
            memset(pAlarmRecordCfg->mediaType.typeName, '\0', RECORD_MEDIA_TYPE_MAX_LEN);
            StrCpy(pAlarmRecordCfg->mediaType.typeName, RECORD_MEDIA_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PreRecordTime"))
        {
            pAlarmRecordCfg->precordTime = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RecordTime"))
        {
            pAlarmRecordCfg->recordTime = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LocalStore"))
        {
            pAlarmRecordCfg->localStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RemoteStore"))
        {
            pAlarmRecordCfg->remoteStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FtpUpload"))
        {
            pAlarmRecordCfg->ftpUpload = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EmailUpload"))
        {
            pAlarmRecordCfg->emailUpload = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StopNoAlarm"))
        {
            pAlarmRecordCfg->stopNoAlarm = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "alarmbits"))
        {
            pAlarmRecordCfg->alarmbits = Str2Num(tmpAttr->nodeValue);
            bGetAlarmBits = 1;
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetAlarmBits)
    {
        for (int iIndex = 0; iIndex < RECORD_ALARM_BIT_MAX; iIndex++)
        {
            BIT_SET_32(pAlarmRecordCfg->alarmbits, iIndex);
        }
    }

    return 0;
}

static int anj_config_record_capture_get(IXML_Node *pNode, AlarmCaptureConfig *pAlarmCaptureCfg)
{
    memset(pAlarmCaptureCfg, 0, sizeof(AlarmCaptureConfig));
    pAlarmCaptureCfg->stream = 1;
    IXML_Node *tmpAttr = NULL;

    int bGetAlarmBits = 0;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PreTakeTime"))
        {
            pAlarmCaptureCfg->preTakeTime = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "sendoutInterval"))
        {
            pAlarmCaptureCfg->sendoutInterval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TotalTakeTime"))
        {
            pAlarmCaptureCfg->totalTakeTime = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "LocalStore"))
        {
            pAlarmCaptureCfg->localStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "RemoteStore"))
        {
            pAlarmCaptureCfg->remoteStore = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FtpUpload"))
        {
            pAlarmCaptureCfg->ftpUpload = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EmailUpload"))
        {
            pAlarmCaptureCfg->emailUpload = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "StopNoAlarm"))
        {
            pAlarmCaptureCfg->stopNoAlarm = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Stream"))
        {
            pAlarmCaptureCfg->stream = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "alarmbits"))
        {
            pAlarmCaptureCfg->alarmbits = Str2Num(tmpAttr->nodeValue);
            bGetAlarmBits = 1;
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetAlarmBits)
    {
        for (int iIndex = 0; iIndex < RECORD_ALARM_BIT_MAX; iIndex++)
        {
            BIT_SET_32(pAlarmCaptureCfg->alarmbits, iIndex);
        }
    }

    return 0;
}

char *anj_config_record_conver_xml(RecordConfig *pRecordCfgArray, int camera_index, int bMsg)
{
    int initSize = 4096;
    char *tmp = NULL;
    int incrSize = 0;
    char escapeBuf[4096];

    char *buf = (char *)anj_mw_malloc(initSize);
    memset(buf, '\0', initSize);
    char *pe = buf + initSize - 1;
    char *pb = buf;
    int curSize = initSize;
    int curPos = 0;

    pb += snprintf(pb, pe - pb, "<RecordConfig>\r\n");
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }
        RecordConfig *pRecordCfg = &pRecordCfgArray[cameraIndex];
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "<Camera id=\"%d\">\r\n", cameraIndex);
        pb += snprintf(pb, pe - pb, "<Common\r\n");
        pb += snprintf(pb, pe - pb, "LocalEnable=\"%d\"\r\n", pRecordCfg->commonCfg.localEnable);
        pb += snprintf(pb, pe - pb, "StorageSequence=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->commonCfg.storageSequence));
        pb += snprintf(pb, pe - pb, "RemoteEnable=\"%d\"\r\n", pRecordCfg->commonCfg.remoteEnable);
        pb += snprintf(pb, pe - pb, "MountParam=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->commonCfg.mountParam));
        pb += snprintf(pb, pe - pb, "StoragePolicy=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->commonCfg.storePolicy.policyName));
        pb += snprintf(pb, pe - pb, "RecordFileSize=\"%d\"\r\n", pRecordCfg->commonCfg.recordFileSize);
        pb += snprintf(pb, pe - pb, "recordFileKeeyDays=\"%d\"\r\n", pRecordCfg->commonCfg.recordFileKeeyDays);
        pb += snprintf(pb, pe - pb, "timelapseEnable=\"%d\"\r\n", pRecordCfg->commonCfg.timelapseCfg.timelapseEnable);
        pb += snprintf(pb, pe - pb, "timelapseSec=\"%d\"\r\n", pRecordCfg->commonCfg.timelapseCfg.timelapseSec);
        pb += snprintf(pb, pe - pb, "timelapseFps=\"%d\"\r\n", pRecordCfg->commonCfg.timelapseCfg.timelapseFps);
        pb += snprintf(pb, pe - pb, "timelapseFileSize=\"%d\"\r\n", pRecordCfg->commonCfg.timelapseCfg.timelapseFileSize);
        pb += snprintf(pb, pe - pb, "/>\r\n");

        pb += snprintf(pb, pe - pb, "<ScheduleRecord\r\n");
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.stream);
        pb += snprintf(pb, pe - pb, "FileFormat=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->scheduleRecordCfg.fileFormat.formatName));
        pb += snprintf(pb, pe - pb, "MediaType=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->scheduleRecordCfg.mediaType.typeName));
        pb += snprintf(pb, pe - pb, "LocalStore=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.localStore);
        pb += snprintf(pb, pe - pb, "RemoteStore=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.remoteStore);

        pb += snprintf(pb, pe - pb, "JpegInterval=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.jpgInterval);
        pb += snprintf(pb, pe - pb, "FtpUpload=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.ftpUpload);
        pb += snprintf(pb, pe - pb, "EmailUpload=\"%d\"\r\n", pRecordCfg->scheduleRecordCfg.emailUpload);

        pb += snprintf(pb, pe - pb, ">\r\n");

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pRecordCfg->scheduleRecordCfg.timeSpan), &timeSpanList);

        curPos = pb - buf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - buf;
        tmp = anj_config_timespan_conver_xml(&(pRecordCfg->scheduleRecordCfg.timeSpan));
        incrSize = strlen(tmp);
        buf = (char *)anj_mw_realloc(buf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(buf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = buf + curPos;
        pe = buf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "</ScheduleRecord>\r\n");

        pb += snprintf(pb, pe - pb, "<MotionDetectRecord\r\n");
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pRecordCfg->motionRecordCfg.stream);
        pb += snprintf(pb, pe - pb, "FileFormat=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->motionRecordCfg.fileFormat.formatName));
        pb += snprintf(pb, pe - pb, "MediaType=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->motionRecordCfg.mediaType.typeName));
        pb += snprintf(pb, pe - pb, "PreRecordTime=\"%d\"\r\n", pRecordCfg->motionRecordCfg.precordTime);
        pb += snprintf(pb, pe - pb, "RecordTime=\"%d\"\r\n", pRecordCfg->motionRecordCfg.recordTime);
        pb += snprintf(pb, pe - pb, "LocalStore=\"%d\"\r\n", pRecordCfg->motionRecordCfg.localStore);
        pb += snprintf(pb, pe - pb, "RemoteStore=\"%d\"\r\n", pRecordCfg->motionRecordCfg.remoteStore);
        pb += snprintf(pb, pe - pb, "FtpUpload=\"%d\"\r\n", pRecordCfg->motionRecordCfg.ftpUpload);
        pb += snprintf(pb, pe - pb, "EmailUpload=\"%d\"\r\n", pRecordCfg->motionRecordCfg.emailUpload);
        pb += snprintf(pb, pe - pb, "alarmbits=\"%u\"\r\n", pRecordCfg->motionRecordCfg.alarmbits);
        pb += snprintf(pb, pe - pb, "/>\r\n");

        pb += snprintf(pb, pe - pb, "<MotionDetectCapture\r\n");
        pb += snprintf(pb, pe - pb, "PreTakeTime=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.preTakeTime);
        pb += snprintf(pb, pe - pb, "sendoutInterval=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.sendoutInterval);
        pb += snprintf(pb, pe - pb, "TotalTakeTime=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.totalTakeTime);
        pb += snprintf(pb, pe - pb, "LocalStore=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.localStore);
        pb += snprintf(pb, pe - pb, "RemoteStore=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.remoteStore);
        pb += snprintf(pb, pe - pb, "FtpUpload=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.ftpUpload);
        pb += snprintf(pb, pe - pb, "EmailUpload=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.emailUpload);
        pb += snprintf(pb, pe - pb, "StopNoAlarm=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.stopNoAlarm);
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pRecordCfg->motionCaptureCfg.stream);
        pb += snprintf(pb, pe - pb, "alarmbits=\"%u\"\r\n", pRecordCfg->motionCaptureCfg.alarmbits);
        pb += snprintf(pb, pe - pb, "/>\r\n");

        pb += snprintf(pb, pe - pb, "<InputAlarmRecord\r\n");
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.stream);
        pb += snprintf(pb, pe - pb, "FileFormat=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->inputAlarmRecordCfg.fileFormat.formatName));
        pb += snprintf(pb, pe - pb, "MediaType=\"%s\"\r\n", copy_with_escape(escapeBuf, pRecordCfg->inputAlarmRecordCfg.mediaType.typeName));
        pb += snprintf(pb, pe - pb, "PreRecordTime=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.precordTime);
        pb += snprintf(pb, pe - pb, "RecordTime=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.recordTime);
        pb += snprintf(pb, pe - pb, "LocalStore=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.localStore);
        pb += snprintf(pb, pe - pb, "RemoteStore=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.remoteStore);
        pb += snprintf(pb, pe - pb, "FtpUpload=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.ftpUpload);
        pb += snprintf(pb, pe - pb, "EmailUpload=\"%d\"\r\n", pRecordCfg->inputAlarmRecordCfg.emailUpload);
        pb += snprintf(pb, pe - pb, "alarmbits=\"%u\"\r\n", pRecordCfg->inputAlarmRecordCfg.alarmbits);
        pb += snprintf(pb, pe - pb, "/>\r\n");

        pb += snprintf(pb, pe - pb, "<InputAlarmCapture\r\n");
        pb += snprintf(pb, pe - pb, "PreTakeTime=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.preTakeTime);
        pb += snprintf(pb, pe - pb, "sendoutInterval=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.sendoutInterval);
        pb += snprintf(pb, pe - pb, "TotalTakeTime=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.totalTakeTime);
        pb += snprintf(pb, pe - pb, "LocalStore=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.localStore);
        pb += snprintf(pb, pe - pb, "RemoteStore=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.remoteStore);
        pb += snprintf(pb, pe - pb, "FtpUpload=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.ftpUpload);
        pb += snprintf(pb, pe - pb, "EmailUpload=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.emailUpload);
        pb += snprintf(pb, pe - pb, "StopNoAlarm=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.stopNoAlarm);
        pb += snprintf(pb, pe - pb, "Stream=\"%d\"\r\n", pRecordCfg->inputAlarmCaptureCfg.stream);
        pb += snprintf(pb, pe - pb, "alarmbits=\"%u\"\r\n", pRecordCfg->inputAlarmCaptureCfg.alarmbits);
        pb += snprintf(pb, pe - pb, "/>\r\n");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "</Camera>\r\n");
    }
    pb += snprintf(pb, pe - pb, "</RecordConfig>\r\n");
    return buf;
}

int anj_config_record_save(RecordConfig *pRecordCfgArray)
{
    int iRet = 0;
    char *pDataXml = anj_config_record_conver_xml(pRecordCfgArray, -1, 0);
    iRet = anj_config_save_node(pDataXml, "<RecordConfig>", "</RecordConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_record_parse_get(IXML_Node *pChildNode, RecordConfig *pRecordCfg)
{
    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "Common"))
        {
            anj_config_record_comm_get(pChildNode, &pRecordCfg->commonCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "ScheduleRecord"))
        {
            anj_config_record_schedule_get(pChildNode, &pRecordCfg->scheduleRecordCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "MotionDetectRecord"))
        {
            anj_config_record_alarm_get(pChildNode, &pRecordCfg->motionRecordCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "InputAlarmRecord"))
        {
            anj_config_record_alarm_get(pChildNode, &pRecordCfg->inputAlarmRecordCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "MotionDetectCapture"))
        {
            anj_config_record_capture_get(pChildNode, &pRecordCfg->motionCaptureCfg);
        }
        else if (!strcmp(pChildNode->nodeName, "InputAlarmCapture"))
        {
            anj_config_record_capture_get(pChildNode, &pRecordCfg->inputAlarmCaptureCfg);
        }

        pChildNode = pChildNode->nextSibling;
    }
    return 0;
}

int anj_config_record_get(IXML_Node *pNode, RecordConfig *pRecordCfgArray)
{
    IXML_Node *pChildNode = pNode->firstChild;
    int camera_index = -1;
    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "Camera"))
        {
            IXML_Node *tmpAttr = pChildNode->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "id"))
                {
                    camera_index = Str2Num(tmpAttr->nodeValue);
                }
                tmpAttr = tmpAttr->nextSibling;
            }
            anj_config_record_parse_get(pChildNode->firstChild, &pRecordCfgArray[camera_index]);
        }
        pChildNode = pChildNode->nextSibling;
    }
    if (camera_index < 0)
    {
        for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
        {
            anj_config_record_parse_get(pNode->firstChild, &pRecordCfgArray[i]);
        }
    }
    return 0;
}

int anj_config_record_set(RecordConfig *pstRecordCfgArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    RecordConfig *recordCfgArray = (RecordConfig *)getRecordConfig();
    if (memcmp(recordCfgArray, pstRecordCfgArray, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS))
    {
        __WARN("Change!!!\n");
        memcpy(recordCfgArray, pstRecordCfgArray, sizeof(RecordConfig) * ANJ_CAMERA_MAX_NUMS);
        anj_config_record_save(pstRecordCfgArray);
        anj_record_restart();
        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_NFS_RESTART, &event_result, NULL);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_record_load(RecordConfig *pRecordCfgArray)
{
    return anj_config_load("RecordConfig", pRecordCfgArray, CONFIG_FILE_PATH);
}

int anj_config_record_get_by_xml(RecordConfig *pRecordCfg, char *xmlBuf, int channel)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "RecordConfig");
    if (pNodelist != NULL)
    {
        anj_config_record_parse_get(pNodelist->nodeItem->firstChild, pRecordCfg);
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
