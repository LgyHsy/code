#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_time.h"
#include "anj_mw_str.h"
#include "anj_config.h"
#include "anj_osd.h"
#include "anj_smart.h"
#include "anj_alarm.h"
#include "anj_comm.h"

#define DEBUG_WRITE_ARMING_TIMESPAN 0

#define ARMING_TIMESPAN_XML_NODENAME_ALL "timespan_all"
#define ARMING_TIMESPAN_XML_NODENAME_AUDIOPLAY "audio_timespan"
#define ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE "timespan_light_twinkle"
#define ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER "timespan_notify_alarmserver"
#define ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED "timespan_alarm_led"
#define ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH "timespan_alarm_push"
#define ARMING_TIMESPAN_XML_NODENAME_NOTIFY_SMS "timespan_notify_sms"

static void anj_config_alarm_camera_get(IXML_Node *pChildNode, int *camera_index, int *LoadCameraIndex)
{
    int camera = *camera_index;
    IXML_Node *tmpAttr = pChildNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "camera"))
        {
            camera = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }
    if (camera < 0)
        camera = 0;
    else if (camera >= ANJ_CAMERA_MAX_NUMS)
        camera = ANJ_CAMERA_MAX_NUMS - 1;
    else
        *LoadCameraIndex = camera + 1;

    *camera_index = camera;
}

static void SetAllDayTimeSpan(ArmingStruct *pOutput)
{
    pOutput->timespan_num = 1;
    memset(pOutput->timeSpans, 0, sizeof(pOutput->timeSpans));
    pOutput->timeSpans[0].endTime.hour = 23;
    pOutput->timeSpans[0].endTime.minute = 59;
    pOutput->timeSpans[0].endTime.sec = 59;
}

static int Arming_getDaytimespans(const IXML_Node *pNode, const char *pNodeNameToBeFind, ArmingStruct *pData)
{
    int bFound = 0;
    if (NULL == pNode)
        return bFound;

    IXML_Node *tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, pNodeNameToBeFind))
        {
            bFound = 1;

            IXML_Node *pNodeSpan = NULL;
            int curIdx = 0;
            pNodeSpan = tmpChild->firstChild;
            while (pNodeSpan)
            {
                if (!strcmp(pNodeSpan->nodeName, "TimeSpan"))
                {
                    anj_config_daytimespan_get(pNodeSpan, &(pData->timeSpans[curIdx]));
                    curIdx++;

                    if (curIdx >= DAY_TIMESPAN_MAX_NUM)
                        break;
                }
                pNodeSpan = pNodeSpan->nextSibling;
            }

            pData->timespan_num = curIdx;

            break;
        }

        tmpChild = tmpChild->nextSibling;
    }

    return bFound;
}

static int getAlgoSensitivitys(IXML_Node *pNode, unsigned char *sensitivitys)
{
    memset(sensitivitys, 0, MAX_AJAIBITS_SIZE);
    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    int algoType = 0;
    int sensitivityValue = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "d"))
        {
            IXML_Node *tmpAttr = NULL;
            tmpAttr = tmpChild->firstAttr;

            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "v"))
                {
                    sensitivityValue = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "t"))
                {
                    algoType = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (algoType >= 0 && algoType < MAX_AJAIBITS_SIZE)
            {
                sensitivitys[algoType] = sensitivityValue;
            }

            childCnt++;

            if (childCnt >= MAX_AJAIBITS_SIZE)
            {
                break;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int PTZ_getPtzPosition(IXML_Node *pNode, PTZPosition *pPtzPosition)
{
    IXML_Node *tmpAttr = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PositionIndex"))
        {
            pPtzPosition->positionIndex = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int PTZ_getPtzPresetAction(IXML_Node *pNode, PositionPreset *pPreset)
{
    IXML_Node *tmpChild = NULL;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "PTZPosition"))
        {
            PTZ_getPtzPosition(tmpChild, &(pPreset->postion));
        }
        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int PTZ_getPtzLoopAction(IXML_Node *pNode, PositionLoop *pPositionLoop)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Interval"))
        {
            pPositionLoop->interval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Duration"))
        {
            pPositionLoop->duration = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    int childCnt = 0;
    int curIdx = 0;
    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "PTZPosition"))
        {
            PTZ_getPtzPosition(tmpChild, &(pPositionLoop->ptzPositions[curIdx]));
            childCnt++;
            curIdx++;
        }
        tmpChild = tmpChild->nextSibling;
    }

    pPositionLoop->positionCount = childCnt;

    return 0;
}

static int PTZ_getPtzWalkAction(IXML_Node *pNode, PositionWalk *pPositionWalk)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Interval"))
        {
            pPositionWalk->interval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "WalkCount"))
        {
            pPositionWalk->walkCount = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    int childCnt = 0;
    int curIdx = 0;
    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "PTZPosition"))
        {
            PTZ_getPtzPosition(tmpChild, &(pPositionWalk->ptzPositions[curIdx]));
            childCnt++;
            curIdx++;
        }
        tmpChild = tmpChild->nextSibling;
    }

    pPositionWalk->positionCount = childCnt;

    return 0;
}

static int anj_config_alarm_ptz_action_get(IXML_Node *pNode, PTZAction *pPtzAction)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pPtzAction->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ActionType"))
        {
            memset(pPtzAction->actionType.actionName, '\0', PTZ_ACTION_TYPE_MAX_LEN);
            StrCpy(pPtzAction->actionType.actionName, PTZ_ACTION_TYPE_MAX_LEN, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    int find = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, pPtzAction->actionType.actionName))
        {
            find = 1;

            if (!strcmp(tmpChild->nodeName, "PresetPosition"))
            {
                PTZ_getPtzPresetAction(tmpChild, &(pPtzAction->action.preset));
            }
            else if (!strcmp(tmpChild->nodeName, "PositionLoop"))
            {
                PTZ_getPtzLoopAction(tmpChild, &(pPtzAction->action.loop));
            }
            else if (!strcmp(tmpChild->nodeName, "PositionWalk"))
            {
                PTZ_getPtzWalkAction(tmpChild, &(pPtzAction->action.walk));
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    if (find == 0)
        return -1;

    return 0;
}

static int anj_config_alarm_output_chn_action_get(IXML_Node *pNode, OutputChannelAction *pOutputChannelAction)
{
    memset(pOutputChannelAction, 0, sizeof(OutputChannelAction));

    IXML_Node *tmpAttr;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pOutputChannelAction->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PortIndex"))
        {
            pOutputChannelAction->portIndex = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (pOutputChannelAction->enable != 0 && pOutputChannelAction->enable != 1)
    {
        pOutputChannelAction->enable = 0;
    }

    return 0;
}

static int anj_config_alarm_output_action_get(IXML_Node *pNode, AlarmOutputAction *pOutputAct)
{
    memset(pOutputAct, 0, sizeof(AlarmOutputAction));
    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    int curIdx = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "OutputChannelAction"))
        {
            anj_config_alarm_output_chn_action_get(tmpChild, &(pOutputAct->outputChnlActions[curIdx]));

            childCnt++;
            curIdx++;

            if (childCnt >= MAX_OUTPUT_CHANENL_COUNT)
            {
                break;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    pOutputAct->channelCnt = childCnt;

    int iIndex;
    for (iIndex = 0; iIndex < MAX_OUTPUT_CHANENL_COUNT; iIndex++)
    {
        pOutputAct->outputChnlActions[iIndex].portIndex = iIndex + 1;
    }

    return 0;
}

static int anj_config_alarm_audio_action_get(IXML_Node *pNode, AudioPlayAction *pAction)
{
    IXML_Node *tmpAttr = NULL;

    memset(pAction, 0, sizeof(AudioPlayAction));
    pAction->times = 1;
    pAction->intervalsecnods = 0;

    int enable_day = 0;
    int enable_night = 0;
    int bGetNightEnable = 0;
    int bGetEnableFlag = 0;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            enable_day = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EnableNight"))
        {
            bGetNightEnable = 1;
            enable_night = (short)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "enable_flag"))
        {
            bGetEnableFlag = 1;
            pAction->enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Times"))
        {
            pAction->times = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Intervel"))
        {
            pAction->intervalsecnods = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "FileName"))
        {
            memset(pAction->filename, '\0', AUDIO_ACTION_LEN_FILENAME);
            StrCpy(pAction->filename, AUDIO_ACTION_LEN_FILENAME, tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetNightEnable)
    {
        enable_night = enable_day;
    }

    if (!bGetEnableFlag)
    {
        if (enable_day)
        {
            if (enable_night)
                pAction->enable.enable_flag = ARMING_ALLDAY;
            else
                pAction->enable.enable_flag = ARMING_DAYTIME;
        }
        else
        {
            if (enable_night)
                pAction->enable.enable_flag = ARMING_NIGHT;
            else
                pAction->enable.enable_flag = ARMING_DISABLE;
        }
    }
    else
    {
        if (pAction->enable.enable_flag == ARMING_CUSTOM)
        {
            int bGetTimeSpan = Arming_getDaytimespans(pNode, ARMING_TIMESPAN_XML_NODENAME_AUDIOPLAY, &(pAction->enable));
            if (!bGetTimeSpan)
            {
                SetAllDayTimeSpan(&pAction->enable);
            }
        }
        else
        {
            SetAllDayTimeSpan(&pAction->enable);
        }
    }

    return 0;
}

static int anj_config_alarm_oldoutput_action_get(IXML_Node *pNode, AlarmOutputAction *pOutputAct)
{
    IXML_Node *tmpAttr;

    pOutputAct->channelCnt = 1;
    OldOutputChannelAction *pOutputChannelAction = &pOutputAct->oldOutputChnlAction;
    pOutputChannelAction->magicNum = 0xAABBCCDD;
    pOutputChannelAction->enable = 0;
    pOutputChannelAction->portIndex = 1;
    pOutputChannelAction->triggerType = 1;
    pOutputChannelAction->duration = 10;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {

            pOutputChannelAction->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "PortIndex"))
        {
            pOutputChannelAction->portIndex = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TriggerType"))
        {
            if (tmpAttr->nodeValue)
            {
                /*这里转换得到的整形，只是字面上的HIGH标识1，LOW标识0.实际的IO输出状态参见TriggerType的定义：
                HIGH:对应常开，平时为0，触发时写1
                LOW:对应常闭，平时为1，触发时写0
                */
                if (strcmp(tmpAttr->nodeValue, "HIGH") == 0)
                {
                    pOutputChannelAction->triggerType = 1;
                }
                else
                {
                    pOutputChannelAction->triggerType = 0;
                }
            }
        }
        else if (!strcmp(tmpAttr->nodeName, "Duration"))
        {
            pOutputChannelAction->duration = Str2Num(tmpAttr->nodeValue);
        }

        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

static int anj_config_alarm_input_chn_get(IXML_Node *pNode, AlarmChannel *pAlarmChnl)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlarmChnl, 0, sizeof(AlarmChannel));

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PortIndex"))
        {
            pAlarmChnl->portIndex = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ChannelType"))
        {
            memset(pAlarmChnl->channelType.name, '\0', CHANNEL_TYPE_NAME_MAX_LEN);
            StrCpy(pAlarmChnl->channelType.name, CHANNEL_TYPE_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TriggerType"))
        {
            memset(pAlarmChnl->triggerType.name, '\0', TRIGGER_TYPE_NAME_MAX_LEN);
            StrCpy(pAlarmChnl->triggerType.name, TRIGGER_TYPE_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlarmChnl->enable = Str2Num(tmpAttr->nodeValue);
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
            TransTimeSpan2New(&timeSpanList, &pAlarmChnl->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlarmChnl->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlarmChnl->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlarmChnl->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_sms"))
                {
                    pAlarmChnl->alarmAction.notify_sms.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlarmChnl->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlarmChnl->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlarmChnl->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlarmChnl->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlarmChnl->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlarmChnl->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlarmChnl->alarmAction.notify_sms.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_SMS, &(pAlarmChnl->alarmAction.notify_sms));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlarmChnl->alarmAction.notify_sms);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlarmChnl->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "PTZAction"))
                {
                    anj_config_alarm_ptz_action_get(tmp, &(pAlarmChnl->alarmAction.ptzAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlarmChnl->alarmAction.audioAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlarmChnl->alarmAction.outputAction));
                }

                /*
                else if(!strcmp(tmp->nodeName, "RecordAction"))
                {
                    Action_getRecordAction(tmp,&(pAlarmChnl->alarmAction.recordAction));
                }
                else if(!strcmp(tmp->nodeName, "OSDDisplayAction"))
                {
                    Action_getOsdAction(tmp, &(pAlarmChnl->alarmAction.osdDisplayAction));
                }
                else if(!strcmp(tmp->nodeName, "PhotoTakeAction"))
                {
                    Action_getPhotoTakeAction(tmp, &(pAlarmChnl->alarmAction.photoTakeAction));
                }
                */

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int anj_config_alarm_output_chn_get(IXML_Node *pNode, OutputChannel *pAlarmChnl)
{
    IXML_Node *tmpAttr;

    int enable_day = 1;
    int enable_night = 1;
    int bGetEnableFlag = 0;

    memset(pAlarmChnl, 0, sizeof(OutputChannel));
    pAlarmChnl->duration = 10;
    pAlarmChnl->enable.enable_flag = ARMING_ALLDAY;
    strcpy(pAlarmChnl->triggerType.name, "HIGH");
    strcpy(pAlarmChnl->channelType.name, "output");
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "PortIndex"))
        {
            pAlarmChnl->portIndex = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "ChannelType"))
        {
            memset(pAlarmChnl->channelType.name, '\0', CHANNEL_TYPE_NAME_MAX_LEN);
            StrCpy(pAlarmChnl->channelType.name, CHANNEL_TYPE_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "TriggerType"))
        {
            memset(pAlarmChnl->triggerType.name, '\0', TRIGGER_TYPE_NAME_MAX_LEN);
            StrCpy(pAlarmChnl->triggerType.name, TRIGGER_TYPE_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Duration"))
        {
            pAlarmChnl->duration = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "enable_flag"))
        {
            bGetEnableFlag = 1;
            pAlarmChnl->enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "DayEnable"))
        {
            enable_day = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "NightEnable"))
        {
            enable_night = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetEnableFlag)
    {
        if (enable_day)
        {
            if (enable_night)
                pAlarmChnl->enable.enable_flag = ARMING_ALLDAY;
            else
                pAlarmChnl->enable.enable_flag = ARMING_DAYTIME;
        }
        else
        {
            if (enable_night)
                pAlarmChnl->enable.enable_flag = ARMING_NIGHT;
            else
                pAlarmChnl->enable.enable_flag = ARMING_DISABLE;
        }
    }
    else
    {
        if (pAlarmChnl->enable.enable_flag == ARMING_CUSTOM)
        {
            int bGetTimeSpan = Arming_getDaytimespans(pNode, "timespan", &(pAlarmChnl->enable));
            if (!bGetTimeSpan)
            {
                SetAllDayTimeSpan(&pAlarmChnl->enable);
            }
        }
        else
        {
            SetAllDayTimeSpan(&pAlarmChnl->enable);
        }
    }

    return 0;
}

static int anj_config_alarm_input_get(IXML_Node *pNode, InputAlarm *pInputAlarm)
{
    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    int curIdx = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmChannel"))
        {
            anj_config_alarm_input_chn_get(tmpChild, &(pInputAlarm->alarmChannels[curIdx]));

            childCnt++;
            curIdx++;

            if (childCnt >= MAX_ALARMCHANNEL_COUNT)
            {
                break;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    pInputAlarm->channelCnt = childCnt;

    return 0;
}

static int anj_config_alarm_output_get(IXML_Node *pNode, OutPutAlarm *pOutputAlarm)
{
    memset(pOutputAlarm, 0, sizeof(OutPutAlarm));

    IXML_Node *tmpChild = NULL;
    int childCnt = 0;
    int curIdx = 0;

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "OutputChannel"))
        {
            anj_config_alarm_output_chn_get(tmpChild, &(pOutputAlarm->outputChannels[curIdx]));

            childCnt++;
            curIdx++;

            if (childCnt >= MAX_OUTPUT_CHANENL_COUNT)
            {
                break;
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    pOutputAlarm->channelCnt = childCnt;

    int iIndex;
    for (iIndex = 0; iIndex < MAX_OUTPUT_CHANENL_COUNT; iIndex++)
    {
        OutputChannel *pAlarmChnl = &pOutputAlarm->outputChannels[iIndex];
        pAlarmChnl->portIndex = iIndex + 1;
        if (strlen(pAlarmChnl->triggerType.name) == 0)
        {
            pAlarmChnl->duration = 10;
            pAlarmChnl->enable.enable_flag = ARMING_ALLDAY;
            strcpy(pAlarmChnl->triggerType.name, "HIGH"); // 常开
            strcpy(pAlarmChnl->channelType.name, "output");
        }
    }

    return 0;
}

int anj_config_alarm_motion_get(IXML_Node *pNode, MotionDetectAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    int bGetArmingFlag = 0;
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = ARMING_DISABLE;
    pAlm->alarmAction.alarm_led_enable.enable_flag = ARMING_DISABLE;
    pAlm->alarmAction.alarm_push.enable_flag = ARMING_ALLDAY;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue) > 0 ? 1 : 0;
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
            bGetArmingFlag = 1;
        }
        else if (!strcmp(tmpAttr->nodeName, "BlockCount"))
        {
            pAlm->blockCount = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "BlockConfig"))
        {
            memset(pAlm->blockCfg, '\0', MAX_MOTIONDETECT_CONFIG_STRING);
            StrCpy(pAlm->blockCfg, MAX_MOTIONDETECT_CONFIG_STRING, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "AlarmThreshold"))
        {
            pAlm->alarmThreshold = Str2Num(tmpAttr->nodeValue);
        }
        // added to support day night motion config
        else if (!strcmp(tmpAttr->nodeName, "DayNightSwitch"))
        {
            pAlm->dayNightSwitch = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "NightSensitivity"))
        {
            pAlm->nightSensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "NightAlarmThreshold"))
        {
            pAlm->nightAlarmThreshold = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "NightStartTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pAlm->nightTime.startTime));
        }
        else if (!strcmp(tmpAttr->nodeName, "NightEndTime"))
        {
            GetTime(tmpAttr->nodeValue, &(pAlm->nightTime.endTime));
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

            TransTimeSpan2New(&timeSpanList, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        /*
        else if(!strcmp(tmpChild->nodeName, "RegionList"))
        {
            MotionDetect_getRegions(tmpChild, &pAlm->regions);
        }
        */
        else if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlm->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_led"))
                {
                    pAlm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_led_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "PTZAction"))
                {
                    anj_config_alarm_ptz_action_get(tmp, &(pAlm->alarmAction.ptzAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                /*
                else if(!strcmp(tmp->nodeName, "RecordAction"))
                {
                    Action_getRecordAction(tmp,&(pAlm->alarmAction.recordAction));
                }
                else if(!strcmp(tmp->nodeName, "OSDDisplayAction"))
                {
                    Action_getOsdAction(tmp, &(pAlm->alarmAction.osdDisplayAction));
                }
                else if(!strcmp(tmp->nodeName, "PhotoTakeAction"))
                {
                    Action_getPhotoTakeAction(tmp, &(pAlm->alarmAction.photoTakeAction));
                }
                */

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }

    if (!bGetArmingFlag) // 兼容老版本配置端
    {
        if (pAlm->enable > 0)
        {
            pAlm->arming_flag = ARMING_ALLDAY;
        }
        else
        {
            pAlm->arming_flag = ARMING_DISABLE;
        }
    }

    int iX = (pAlm->blockCount & 0xffff0000) >> 16;
    int iY = (pAlm->blockCount & 0xffff);

    if (strcmp(pAlm->blockCfg, "") == 0)
    {
        int i;
        memset(pAlm->blockCfg, 0, sizeof(pAlm->blockCfg));
        for (i = 0; i < MD_MAX_GRID_COL * MD_MAX_GRID_ROW; i++)
        {
            if (i < iX * iY)
            {
                pAlm->blockCfg[i] = '1';
            }
        }
    }
    return 0;
}

static int anj_config_alarm_video_lost_get(IXML_Node *pNode, VideoLostAlarm *pVideoLostAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pVideoLostAlm->enable = Str2Num(tmpAttr->nodeValue);
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
            TransTimeSpan2New(&timeSpanList, &pVideoLostAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pVideoLostAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pVideoLostAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pVideoLostAlm->alarmAction.outputAction));
                }
                /*
                else if(!strcmp(tmp->nodeName, "OSDDisplayAction"))
                {
                    Action_getOsdAction(tmp, &(pVideoLostAlm->alarmAction.osdDisplayAction));
                }
                */

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }
    return 0;
}

static int anj_config_alarm_video_cover_get(IXML_Node *pNode, VideoCoverAlarm *pVideoCoverAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;
    memset(pVideoCoverAlm, 0, sizeof(VideoCoverAlarm));
    pVideoCoverAlm->enable = 0;
    pVideoCoverAlm->sensitivity = 20;
    pVideoCoverAlm->threadhold_second = 5;
    pVideoCoverAlm->backgroundUpdateSecond = 40;
    SetAllTimeSpan(&pVideoCoverAlm->timeSpan);

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pVideoCoverAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pVideoCoverAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "threadhold_second"))
        {
            pVideoCoverAlm->threadhold_second = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "backgroundUpdateSecond"))
        {
            pVideoCoverAlm->backgroundUpdateSecond = Str2Num(tmpAttr->nodeValue);
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
            TransTimeSpan2New(&timeSpanList, &pVideoCoverAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pVideoCoverAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pVideoCoverAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pVideoCoverAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pVideoCoverAlm->alarmAction.audioAction));
                }
                /*
                else if(!strcmp(tmp->nodeName, "OSDDisplayAction"))
                {
                    Action_getOsdAction(tmp, &(pVideoCoverAlm->alarmAction.osdDisplayAction));
                }
                 */

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }
    return 0;
}

static int anj_config_alarm_storage_full_get(IXML_Node *pNode, StorageFullAlarm *pStorageFullAlm)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pStorageFullAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Threshold"))
        {
            pStorageFullAlm->threshold = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pStorageFullAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pStorageFullAlm->alarmAction.outputAction));
                }
                /*
                else if(!strcmp(tmp->nodeName, "OSDDisplayAction"))
                {
                    Action_getOsdAction(tmp, &(pStorageFullAlm->alarmAction.osdDisplayAction));
                }
                */
                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

static int anj_config_alarm_audio_get(IXML_Node *pNode, AudioAlarm *pAlm)
{
    IXML_Node *tmpAttr;
    memset(pAlm, 0, sizeof(AudioAlarm));
    pAlm->sensity_babycry = 50;
    pAlm->sensity_lsd = 50;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "enable_babycry"))
        {
            pAlm->enable_babycry = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "sensity_babycry"))
        {
            pAlm->sensity_babycry = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "enable_lsd"))
        {
            pAlm->enable_lsd = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "sensity_lsd"))
        {
            pAlm->sensity_lsd = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

int anj_config_alarm_video_gate_get(IXML_Node *pNode, VideoGateAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    int bGetArmingFlag = 0;
    memset(pAlm, 0, sizeof(VideoGateAlarm));
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->alarmAction.alarm_push.enable_flag = ARMING_ALLDAY;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
            bGetArmingFlag = 1;
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            char szNodeName[32];
            sprintf(szNodeName, "Line%d", iIndex + 1);
            if (0 == strcmp(tmpChild->nodeName, szNodeName))
            {
                int enable_day = 0;
                int enable_night = 0;
                int bGetNewFlag = 0;

                IXML_Node *tmp;
                tmp = tmpChild->firstAttr;
                while (tmp)
                {
                    if (!strcmp(tmp->nodeName, "fromx"))
                    {
                        pAlm->data[iIndex].x0Pos = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "fromy"))
                    {
                        pAlm->data[iIndex].y0Pos = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "tox"))
                    {
                        pAlm->data[iIndex].x1Pos = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "toy"))
                    {
                        pAlm->data[iIndex].y1Pos = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "EnableDay"))
                    {
                        enable_day = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "EnableNight"))
                    {
                        enable_night = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "Enable"))
                    {
                        pAlm->data[iIndex].enable = Str2Num(tmp->nodeValue);
                        bGetNewFlag = 1;
                    }
                    else if (!strcmp(tmp->nodeName, "sensitivity"))
                    {
                        pAlm->data[iIndex].sensitivity = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "type"))
                    {
                        pAlm->data[iIndex].type = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "direction"))
                    {
                        pAlm->data[iIndex].direction = Str2Num(tmp->nodeValue);
                    }

                    tmp = tmp->nextSibling;
                }

                if (!bGetNewFlag)
                {
                    pAlm->data[iIndex].enable = (enable_day > 0 || enable_night > 0) ? 1 : 0;
                }
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    if (!bGetArmingFlag) // 兼容老配置，只要任意一条规则开启，则将算法开关打开
    {
        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            if (pAlm->data[iIndex].enable > 0)
            {
                pAlm->enable = 1;
                break;
            }
        }
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "draw_target"))
                {
                    pAlm->alarmAction.draw_target_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlm->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_led"))
                {
                    pAlm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_led_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }

                tmp = tmp->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "EnableTimeList"))
        {
            TimeSpanList timeSpanList;
            anj_config_timespan_list_get(tmpChild, &timeSpanList);
            TransTimeSpan2New(&timeSpanList, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        tmpChild = tmpChild->nextSibling;
    }
    return 0;
}

static int anj_config_alarm_temp_humidity_get(IXML_Node *pNode, TempHumidityAlarm *pAlm)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlm, 0, sizeof(TempHumidityAlarm));
    // 初始化的值

    pAlm->temp_enable = 0;
    pAlm->temp_upper_limit = 70;
    pAlm->temp_lower_limit = 0;
    pAlm->temp_range_lower = -50;
    pAlm->temp_range_upper = 100;

    pAlm->humidity_enable = 0;
    pAlm->humidity_upper_limit = 90;
    pAlm->humidity_lower_limit = 15;
    pAlm->humidity_range_lower = 0;
    pAlm->humidity_range_upper = 100;

    pAlm->voc_enable = 0;
    pAlm->voc_threashhold_good = 500;
    pAlm->voc_threashhold_TracePollution = 1000;
    pAlm->voc_threashhold_LightPollution = 2000;
    pAlm->voc_threashhold_ModeratePollution = 3000;
    pAlm->voc_threashhold_HeavyPollution = 5; // 用做告警等级

    //  pAlm->enable= 1;
    pAlm->alarmAction.outputAction.channelCnt = 1;
    pAlm->alarmAction.outputAction.outputChnlActions[0].enable = 0;
    pAlm->alarmAction.outputAction.outputChnlActions[0].portIndex = 1;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "temp_upper_limit"))
        {
            pAlm->temp_upper_limit = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "temp_lower_limit"))
        {
            pAlm->temp_lower_limit = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "temp_range_lower"))
        {
            pAlm->temp_range_lower = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "temp_range_upper"))
        {
            pAlm->temp_range_upper = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "temp_enable"))
        {
            pAlm->temp_enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "humidity_upper_limit"))
        {
            pAlm->humidity_upper_limit = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "humidity_lower_limit"))
        {
            pAlm->humidity_lower_limit = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "humidity_range_lower"))
        {
            pAlm->humidity_range_lower = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "humidity_range_upper"))
        {
            pAlm->humidity_range_upper = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "humidity_enable"))
        {
            pAlm->humidity_enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_enable"))
        {
            pAlm->voc_enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_good"))
        {
            pAlm->voc_threashhold_good = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_TracePollution"))
        {
            pAlm->voc_threashhold_TracePollution = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_LightPollution"))
        {
            pAlm->voc_threashhold_LightPollution = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_ModeratePollution"))
        {
            pAlm->voc_threashhold_ModeratePollution = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "voc_HeavyPollution"))
        {
            pAlm->voc_threashhold_HeavyPollution = Str2Num(tmpAttr->nodeValue);
        }

        //      else if(!strcmp(tmpAttr->nodeName, "enable"))
        //      {
        //          pAlm->enable= Str2Num(tmpAttr->nodeValue);
        //      }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    // anj_config_alarm_oldoutput_action_get(tmp,&(pAlm->alarmAction.outputAction));
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }

                tmp = tmp->nextSibling;
            }
        }
        tmpChild = tmpChild->nextSibling;
    }

    return 0;
}

int anj_config_alarm_pd_get(IXML_Node *pNode, PdAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr = NULL;
    IXML_Node *tmpChild = NULL;
    memset(pAlm, 0, sizeof(PdAlarm));
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->minTargetRate = 0;
    pAlm->nonMotionFilter = 0;
    pAlm->allowMd = SMART_MD_DEFAULT_ENABLE;
    pAlm->type = 0;
    BIT_SET_32(pAlm->type, AI_TYPE_BIT_HUMAN); // 先把人形选上

    pAlm->alarmAction.draw_rect_enable = 1;
    pAlm->alarmAction.draw_human_enable = 1;
    pAlm->alarmAction.track_human_enable = 0;
    pAlm->alarmAction.rect_twinkle_enable = 0;
    pAlm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = ARMING_DISABLE;
    pAlm->alarmAction.alarm_led_enable.enable_flag = ARMING_ALLDAY;
    pAlm->alarmAction.alarm_push.enable_flag = ARMING_ALLDAY;
    pAlm->alarmAction.auto_zoom_enable = 0;
    pAlm->alarmAction.track_time = 25;
    pAlm->alarmAction.gunball_track_mode = 0xff;

    pAlm->area.xPos = 0;
    pAlm->area.yPos = 0;
    pAlm->area.width = 100;
    pAlm->area.height = 100;
    pAlm->alarmAction.outputAction.channelCnt = 1;
    pAlm->alarmAction.outputAction.outputChnlActions[0].enable = 0;
    pAlm->alarmAction.outputAction.outputChnlActions[0].portIndex = 1;
    pAlm->threshold = 75;
    pAlm->sensitivity = 6;

    int enable_day = 0;
    int enable_night = 0;
    int bGetArmingFlag = 0;
    int bGetSensitivitysFlag = 0;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
            bGetArmingFlag = 1;
        }
        else if (!strcmp(tmpAttr->nodeName, "EnableDay"))
        {
            enable_day = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EnableNight"))
        {
            enable_night = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Type"))
        {
            pAlm->type = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Threshold"))
        {
            pAlm->threshold = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "minTargetRate"))
        {
            pAlm->minTargetRate = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "nonMotionFilter"))
        {
            pAlm->nonMotionFilter = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "allowMd"))
        {
            pAlm->allowMd = (unsigned char)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "xPos"))
        {
            pAlm->area.xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "yPos"))
        {
            pAlm->area.yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "width"))
        {
            pAlm->area.width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "height"))
        {
            pAlm->area.height = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetArmingFlag) // 兼容老版本配置端
    {
        if (enable_day || enable_night)
            pAlm->enable = 1;

        if (enable_day)
        {
            if (enable_night)
                pAlm->arming_flag = ARMING_ALLDAY;
            else
                pAlm->arming_flag = ARMING_DAYTIME;
        }
        else
        {
            if (enable_night)
                pAlm->arming_flag = ARMING_NIGHT;
            else
                pAlm->arming_flag = ARMING_DISABLE;
        }
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "draw_human"))
                {
                    pAlm->alarmAction.draw_human_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "track_human"))
                {
                    pAlm->alarmAction.track_human_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "track_time"))
                {
                    pAlm->alarmAction.track_time = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "auto_zoom"))
                {
                    pAlm->alarmAction.auto_zoom_enable = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "gunball_track_mode"))
                {
                    pAlm->alarmAction.gunball_track_mode = (unsigned char)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "rect_twinkle"))
                {
                    pAlm->alarmAction.rect_twinkle_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlm->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_led"))
                {
                    pAlm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_led_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }
                tmp = tmp->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "Sensitivitys"))
        {
            getAlgoSensitivitys(tmpChild, pAlm->sensitivitys);
            bGetSensitivitysFlag = 1;
        }
        else if (!strcmp(tmpChild->nodeName, "EnableTimeList"))
        {
            TimeSpanList timeSpanList;
            anj_config_timespan_list_get(tmpChild, &timeSpanList);
            TransTimeSpan2New(&timeSpanList, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "Polygon"))
        {
            anj_config_polygon_get(tmpChild, &pAlm->polygonArea);
        }
        tmpChild = tmpChild->nextSibling;
    }

    if (!bGetSensitivitysFlag)
    {
        ////历史原因最早算法灵敏度这个值范围是0-10。
        // 配置界面上将sensitivity作为10位/threshold作为个位组合起来成为一个0-100的数来展示
        // 新加的灵敏度值范围需要用0-100
        int calValue = (pAlm->sensitivity % 10) * 10 + (pAlm->threshold % 10);
        int index = 0;
        for (index = 0; index < MAX_AJAIBITS_SIZE; index++)
        {
            pAlm->sensitivitys[index] = calValue;
        }
    }
    else
    {
        pAlm->sensitivity = pAlm->sensitivitys[AI_TYPE_BIT_HUMAN] / 10;
        pAlm->threshold = pAlm->sensitivitys[AI_TYPE_BIT_HUMAN] % 10;
    }

    anj_config_polygon_check(&pAlm->polygonArea);

    if (pAlm->type == 0)
    {
        BIT_SET_32(pAlm->type, AI_TYPE_BIT_HUMAN); // 先把人形选上
    }
    return 0;
}

static int anj_config_alarm_fd_get(IXML_Node *pNode, FaceDetectAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlm, 0, sizeof(FaceDetectAlarm));
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->alarmAction.draw_rect_enable = 1;
    pAlm->area.xPos = 0;
    pAlm->area.yPos = 0;
    pAlm->area.width = 100;
    pAlm->area.height = 100;
    pAlm->alarmAction.outputAction.channelCnt = 1;
    pAlm->alarmAction.outputAction.outputChnlActions[0].enable = 0;
    pAlm->alarmAction.outputAction.outputChnlActions[0].portIndex = 1;
    pAlm->threshold = 75;
    pAlm->sensitivity = 6;

    int enable_day = 0;
    int enable_night = 0;
    int bGetArmingFlag = 0;

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
            bGetArmingFlag = 1;
        }
        else if (!strcmp(tmpAttr->nodeName, "EnableDay"))
        {
            enable_day = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "EnableNight"))
        {
            enable_night = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Threshold"))
        {
            pAlm->threshold = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "xPos"))
        {
            pAlm->area.xPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "yPos"))
        {
            pAlm->area.yPos = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "width"))
        {
            pAlm->area.width = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "height"))
        {
            pAlm->area.height = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    if (!bGetArmingFlag) // 兼容老版本配置端
    {
        if (enable_day || enable_night)
            pAlm->enable = 1;

        if (enable_day)
        {
            if (enable_night)
                pAlm->arming_flag = ARMING_ALLDAY;
            else
                pAlm->arming_flag = ARMING_DAYTIME;
        }
        else
        {
            if (enable_night)
                pAlm->arming_flag = ARMING_NIGHT;
            else
                pAlm->arming_flag = ARMING_DISABLE;
        }
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }

                tmp = tmp->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "EnableTimeList"))
        {
            TimeSpanList timeSpanList;
            anj_config_timespan_list_get(tmpChild, &timeSpanList);
            TransTimeSpan2New(&timeSpanList, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        tmpChild = tmpChild->nextSibling;
    }
    return 0;
}

static int anj_config_alarm_lpr_get(IXML_Node *pNode, LprAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlm, 0, sizeof(LprAlarm));
    pAlm->enable = 0;
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->sensitivity = 60;
    SetAllTimeSpan(&pAlm->timeSpan);

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "detectionmode"))
        {
            pAlm->detectionmode = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "actionInterval"))
        {
            pAlm->actionInterval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "snapQuality"))
        {
            StrCpy(pAlm->snapQuality, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "draw_target"))
                {
                    pAlm->alarmAction.draw_target_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "drawosd"))
                {
                    pAlm->alarmAction.draw_osd_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "playvoice"))
                {
                    pAlm->alarmAction.play_voice_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "Polygon"))
        {
            anj_config_polygon_get(tmpChild, &pAlm->polygonArea);
        }
        tmpChild = tmpChild->nextSibling;
    }

    anj_config_polygon_check(&pAlm->polygonArea);
    return 0;
}

static int anj_config_alarm_fire_get(IXML_Node *pNode, FlameAndFlumesAlarm *pAlm)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlm, 0, sizeof(LprAlarm));
    pAlm->enable = 0;
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->sensitivity = 60;
    SetAllTimeSpan(&pAlm->timeSpan);

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity"))
        {
            pAlm->sensitivity = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "Sensitivity_smog"))
        {
            pAlm->sensitivity_smog = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "actionInterval"))
        {
            pAlm->actionInterval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "snapQuality"))
        {
            StrCpy(pAlm->snapQuality, RESOLUTION_NAME_MAX_LEN, tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "draw_target"))
                {
                    pAlm->alarmAction.draw_target_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlm->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_led"))
                {
                    pAlm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_led_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }
                tmp = tmp->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "Polygon"))
        {
            anj_config_polygon_get(tmpChild, &pAlm->polygonArea);
        }
        tmpChild = tmpChild->nextSibling;
    }

    anj_config_polygon_check(&pAlm->polygonArea);

    return 0;
}

//
static int anj_config_alarm_region_ai_get(IXML_Node *pNode, VideoRegionAiAlarm *pAlm, int bMsg)
{
    IXML_Node *tmpAttr;
    IXML_Node *tmpChild;

    memset(pAlm, 0, sizeof(VideoRegionAiAlarm));
    pAlm->arming_flag = ARMING_ALLDAY;
    pAlm->alarmAction.alarm_push.enable_flag = ARMING_ALLDAY;
    pAlm->alarmAction.outputAction.channelCnt = 1;
    pAlm->alarmAction.outputAction.outputChnlActions[0].enable = 0;
    pAlm->alarmAction.outputAction.outputChnlActions[0].portIndex = 1;

    int bGetArmingFlag = 0;
    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "Enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "arming_flag"))
        {
            pAlm->arming_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
            bGetArmingFlag = 1;
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_REGION_AI_NUM; iIndex++)
        {
            char szNodeName[32];
            sprintf(szNodeName, "Rule%d", iIndex + 1);
            if (0 == strcmp(tmpChild->nodeName, szNodeName))
            {
                IXML_Node *tmp;
                tmp = tmpChild->firstAttr;
                while (tmp)
                {
                    if (!strcmp(tmp->nodeName, "enable"))
                    {
                        pAlm->data[iIndex].enable = Str2Num(tmp->nodeValue) > 0 ? 1 : 0;
                    }
                    else if (!strcmp(tmp->nodeName, "sensitivity"))
                    {
                        pAlm->data[iIndex].sensitivity = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "type_filter"))
                    {
                        pAlm->data[iIndex].type_filter = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "type_filter"))
                    {
                        pAlm->data[iIndex].type_filter = Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "mode"))
                    {
                        pAlm->data[iIndex].mode = (RegionAiMode)Str2Num(tmp->nodeValue);
                    }
                    else if (!strcmp(tmp->nodeName, "stayseconds"))
                    {
                        pAlm->data[iIndex].stayseconds = Str2Num(tmp->nodeValue);
                    }

                    tmp = tmp->nextSibling;
                }
            }
        }

        tmpChild = tmpChild->nextSibling;
    }

    if (!bGetArmingFlag) // 兼容老配置，只要任意一条规则开启，则将算法开关打开
    {
        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            if (pAlm->data[iIndex].enable > 0)
            {
                pAlm->enable = 1;
                break;
            }
        }
    }

    tmpChild = pNode->firstChild;
    while (tmpChild)
    {
        if (!strcmp(tmpChild->nodeName, "AlarmAction"))
        {
            tmpAttr = tmpChild->firstAttr;
            while (tmpAttr)
            {
                if (!strcmp(tmpAttr->nodeName, "drawrect"))
                {
                    pAlm->alarmAction.draw_rect_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "draw_target"))
                {
                    pAlm->alarmAction.draw_target_enable = Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "light_twinkle"))
                {
                    pAlm->alarmAction.light_twinkle_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "notify_alarmserver"))
                {
                    pAlm->alarmAction.notify_alarmserver_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_led"))
                {
                    pAlm->alarmAction.alarm_led_enable.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }
                else if (!strcmp(tmpAttr->nodeName, "alarm_push"))
                {
                    pAlm->alarmAction.alarm_push.enable_flag = (ArmingMode)Str2Num(tmpAttr->nodeValue);
                }

                tmpAttr = tmpAttr->nextSibling;
            }

            if (pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.light_twinkle_enable);
                }
            }
            if (pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.notify_alarmserver_enable);
                }
            }
            if (pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_led_enable);
                }
            }
            if (pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
            {
                int bGetTimeSpan = Arming_getDaytimespans(tmpChild, ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
                if (!bGetTimeSpan)
                {
                    SetAllDayTimeSpan(&pAlm->alarmAction.alarm_push);
                }
            }

            IXML_Node *tmp = tmpChild->firstChild;
            while (tmp)
            {
                if (!strcmp(tmp->nodeName, "IOOutputAction"))
                {
                    anj_config_alarm_output_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                {
                    anj_config_alarm_oldoutput_action_get(tmp, &(pAlm->alarmAction.outputAction));
                }
                else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                {
                    anj_config_alarm_audio_action_get(tmp, &(pAlm->alarmAction.audioAction));
                }

                tmp = tmp->nextSibling;
            }
        }
        else if (!strcmp(tmpChild->nodeName, "EnableTimeList"))
        {
            TimeSpanList timeSpanList;
            anj_config_timespan_list_get(tmpChild, &timeSpanList);
            TransTimeSpan2New(&timeSpanList, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "TimeSpanCfg"))
        {
            anj_config_timespan_get(tmpChild, &pAlm->timeSpan);
        }
        else if (!strcmp(tmpChild->nodeName, "Polygon"))
        {
            anj_config_polygon_get(tmpChild, &pAlm->polygonArea);
        }
        tmpChild = tmpChild->nextSibling;
    }
    anj_config_polygon_check(&pAlm->polygonArea);
    return 0;
}

static int anj_config_alarm_sms_get(IXML_Node *pNode, SMSAlarm *pAlm)
{
    IXML_Node *tmpAttr;

    memset(pAlm, 0, sizeof(SMSAlarm));

    tmpAttr = pNode->firstAttr;
    while (tmpAttr)
    {
        if (!strcmp(tmpAttr->nodeName, "enable"))
        {
            pAlm->enable = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "dstnumber"))
        {
            StrCpy(pAlm->szSmsDstNum, MAX_SMS_DESTNUM_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "content"))
        {
            StrCpy(pAlm->szSmsFixContent, MAX_SMS_DESTNUM_LEN, tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "min_interval"))
        {
            pAlm->min_interval = Str2Num(tmpAttr->nodeValue);
        }
        else if (!strcmp(tmpAttr->nodeName, "playresult"))
        {
            pAlm->playresult = Str2Num(tmpAttr->nodeValue);
        }
        tmpAttr = tmpAttr->nextSibling;
    }

    return 0;
}

char *anj_config_alarm_arming_daytimesapn_conver_xml(const char *TimeSpanName, ArmingStruct *pData)
{
    int initSize = 512;
    char *pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);
    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<%s>\r\n", TimeSpanName);

    unsigned int i = 0;
    char buf[12];
    for (; i < pData->timespan_num && i < DAY_TIMESPAN_MAX_NUM; i++)
    {
        pb += snprintf(pb, pe - pb, "<TimeSpan ");
        GetDayTimeStr(buf, 12, &(pData->timeSpans[i].startTime));
        pb += snprintf(pb, pe - pb, "StartTime=\"%s\" ", buf);
        GetDayTimeStr(buf, 12, &(pData->timeSpans[i].endTime));
        pb += snprintf(pb, pe - pb, "EndTime=\"%s\" ", buf);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</%s>\r\n", TimeSpanName);

    return pBuf;
}

char *anj_config_alarm_audio_action_conver_xml(AudioPlayAction *pAction)
{
    char *pBuf = NULL;
    int initSize = 1024;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;

    int enable_day = 0;
    int enable_night = 0;
    if (pAction->enable.enable_flag == ARMING_ALLDAY || pAction->enable.enable_flag == ARMING_DAYTIME || pAction->enable.enable_flag == ARMING_CUSTOM)
    {
        enable_day = 1;
    }
    if (pAction->enable.enable_flag == ARMING_ALLDAY || pAction->enable.enable_flag == ARMING_NIGHT || pAction->enable.enable_flag == ARMING_CUSTOM)
    {
        enable_night = 1;
    }

    pb += snprintf(pb, pe - pb, "<AudioPlayAction ");

    pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", enable_day);
    pb += snprintf(pb, pe - pb, "EnableNight=\"%d\" ", enable_night);
    pb += snprintf(pb, pe - pb, "enable_flag=\"%d\" ", pAction->enable.enable_flag);
    pb += snprintf(pb, pe - pb, "Times=\"%d\" ", pAction->times);
    pb += snprintf(pb, pe - pb, "Intervel=\"%d\" ", pAction->intervalsecnods);
    pb += snprintf(pb, pe - pb, "FileName=\"%s\" >", pAction->filename);
    pb += snprintf(pb, pe - pb, "\r\n");

    curPos = pb - pBuf;
    if (DEBUG_WRITE_ARMING_TIMESPAN || pAction->enable.enable_flag == ARMING_CUSTOM)
    {
        curPos = pb - pBuf;
        tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_AUDIOPLAY, &(pAction->enable));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    pb += snprintf(pb, pe - pb, "</AudioPlayAction>\r\n");

    return pBuf;
}

char *anj_config_alarm_ptz_action_preset_conver_xml(PositionPreset *pPreset)
{
    char *pBuf = NULL;
    int bufSize = 1024;

    pBuf = (char *)anj_mw_malloc(bufSize);
    memset(pBuf, '\0', bufSize);

    char *pb = pBuf;
    char *pe = pBuf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<PresetPosition>\r\n");
    pb += snprintf(pb, pe - pb, "<PTZPosition\r\n");
    pb += snprintf(pb, pe - pb, "PositionIndex=\"%d\"\r\n", pPreset->postion.positionIndex);
    pb += snprintf(pb, pe - pb, "/>\r\n");
    pb += snprintf(pb, pe - pb, "</PresetPosition>\r\n");

    return pBuf;
}

char *anj_config_alarm_ptz_action_loop_conver_xml(PositionLoop *pLoop)
{
    char *pBuf = NULL;
    int i = 0;
    int sizePerPos = 100;
    int totalSize = pLoop->positionCount * sizePerPos + 100;

    pBuf = (char *)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<PositionLoop\r\n");
    pb += snprintf(pb, pe - pb, "Interval=\"%d\"\r\n", pLoop->interval);
    pb += snprintf(pb, pe - pb, "Duration=\"%d\"\r\n", pLoop->duration);
    pb += snprintf(pb, pe - pb, ">\r\n");

    for (i = 0; i < pLoop->positionCount; i++)
    {
        pb += snprintf(pb, pe - pb, "<PTZPosition\r\n");
        pb += snprintf(pb, pe - pb, "PositionIndex=\"%d\"\r\n", pLoop->ptzPositions[i].positionIndex);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</PositionLoop>\r\n");

    return pBuf;
}

char *anj_config_alarm_ptz_action_walk_conver_xml(PositionWalk *pWalk)
{
    char *pBuf = NULL;
    int i = 0;
    int sizePerPos = 100;
    int totalSize = pWalk->positionCount * sizePerPos + 100;

    pBuf = (char *)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<PositionWalk\r\n");
    pb += snprintf(pb, pe - pb, "Interval=\"%d\"\r\n", pWalk->interval);
    pb += snprintf(pb, pe - pb, "WalkCount=\"%d\"\r\n", pWalk->walkCount);
    pb += snprintf(pb, pe - pb, ">\r\n");

    for (i = 0; i < pWalk->positionCount; i++)
    {
        pb += snprintf(pb, pe - pb, "<PTZPosition\r\n");
        pb += snprintf(pb, pe - pb, "PositionIndex=\"%d\"\r\n", pWalk->ptzPositions[i].positionIndex);
        pb += snprintf(pb, pe - pb, "/>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</PositionWalk>\r\n");

    return pBuf;
}

char *anj_config_alarm_output_action_conver_xml(AlarmOutputAction *pAlmOutputAction)
{
    char *pBuf;
    int initSize = 1000;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);
    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;
    int chn = 0;

    pb += snprintf(pb, pe - pb, "<IOOutputAction>\r\n");
    for (chn = 0; chn < MAX_OUTPUT_CHANENL_COUNT; chn++)
    {
        OutputChannelAction *pOutChannel = &pAlmOutputAction->outputChnlActions[chn];
        pb += snprintf(pb, pe - pb, "<OutputChannelAction Enable=\"%d\" PortIndex=\"%d\" /> \r\n",
                       pOutChannel->enable, pOutChannel->portIndex);
    }

    pb += snprintf(pb, pe - pb, "</IOOutputAction>\r\n");
    return pBuf;
}

char *anj_config_alarm_ptz_action_conver_xml(PTZAction *pPtzAction)
{
    char *pBuf;
    int initSize = 1000;
    char escapeBuf[1000];

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);
    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;
    int curPos = 0;
    int incrSize;
    int curSize = initSize;
    char *tmp = NULL;

    pb += snprintf(pb, pe - pb, "<PTZAction\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pPtzAction->enable);
    pb += snprintf(pb, pe - pb, "ActionType=\"%s\"\r\n", copy_with_escape(escapeBuf, pPtzAction->actionType.actionName));
    pb += snprintf(pb, pe - pb, ">\r\n");

    curPos = pb - pBuf;

    if (!strcmp(pPtzAction->actionType.actionName, "PositionLoop"))
    {
        tmp = anj_config_alarm_ptz_action_loop_conver_xml(&(pPtzAction->action.loop));
    }
    else if (!strcmp(pPtzAction->actionType.actionName, "PresetPosition"))
    {
        tmp = anj_config_alarm_ptz_action_preset_conver_xml(&(pPtzAction->action.preset));
    }
    else if (!strcmp(pPtzAction->actionType.actionName, "PositionWalk"))
    {
        tmp = anj_config_alarm_ptz_action_walk_conver_xml(&(pPtzAction->action.walk));
    }
    else
    {
        __ERR("unknown pzt action type %s\n", pPtzAction->actionType.actionName);
        anj_mw_free(pBuf);
        return NULL;
    }

    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</PTZAction>\r\n");

    return pBuf;
}

/*
char *MakeDetectRegionListXml(DetectRegionList *pRegionList)
{
    char* pBuf = NULL;
    int sizePerRegion = 50;
    int totalSize = pRegionList->regionCnt * sizePerRegion + 50;

    pBuf = (char*)anj_mw_malloc(totalSize);
    memset(pBuf, '\0', totalSize);

    char *pe = pBuf + totalSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe-pb, "<RegionList>\r\n");

    int i;
    for(i = 0; i < pRegionList->regionCnt; i++)
    {
        pb += snprintf(pb, pe-pb, "<DetectRegion\r\n");
        pb += snprintf(pb, pe-pb, "Left=\"%d\"\r\n", pRegionList->regions[i].left);
        pb += snprintf(pb, pe-pb, "Top=\"%d\"\r\n", pRegionList->regions[i].top);
        pb += snprintf(pb, pe-pb, "Right=\"%d\"\r\n", pRegionList->regions[i].right);
        pb += snprintf(pb, pe-pb, "Bottom=\"%d\"\r\n", pRegionList->regions[i].bottom);
        pb += snprintf(pb, pe-pb, "DetectInterval=\"%d\"\r\n", pRegionList->regions[i].detectInterval);
        pb += snprintf(pb, pe-pb, "VectorThreshold=\"%d\"\r\n", pRegionList->regions[i].vectorThreshold);
        pb += snprintf(pb, pe-pb, "SADThreshold=\"%d\"\r\n", pRegionList->regions[i].SAGThreshold);
        pb += snprintf(pb, pe-pb, "DEVThreshold=\"%d\"\r\n", pRegionList->regions[i].DEVThreshold);
        pb += snprintf(pb, pe-pb, "AlarmThreshold=\"%d\"\r\n", pRegionList->regions[i].alarmThreshold);
        pb += snprintf(pb, pe-pb, "/>\r\n");
    }

    pb += snprintf(pb, pe-pb, "</RegionList>\r\n");

    return pBuf;
}
char* MakeRecordActionXml(RecordAction *pRecordAct)
{
    char *pBuf = NULL;
    int bufSize = 1000;
    char escapeBuf[1000];

    pBuf = (char*)anj_mw_malloc(bufSize);
    memset(pBuf, '\0', bufSize);

    char *pb = pBuf;
    char *pe = pBuf + bufSize - 1;

    pb += snprintf(pb, pe-pb, "<RecordAction\r\n");
    pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n", pRecordAct->enable);
    pb += snprintf(pb, pe-pb, "PreRecordTime=\"%d\"\r\n", pRecordAct->precordTime);
    pb += snprintf(pb, pe-pb, "RecordTime=\"%d\"\r\n", pRecordAct->recordTime);
    pb += snprintf(pb, pe-pb, "FtpUpload=\"%d\"\r\n", pRecordAct->ftpUpload);
    pb += snprintf(pb, pe-pb, "EmailUpload=\"%d\"\r\n", pRecordAct->emailUpload);
    pb += snprintf(pb, pe-pb, "/>\r\n");

    return pBuf;
}
*/

char *anj_config_alarm_storage_full_conver_xml(StorageFullAlarm *pStorageFullAlm)
{
    char *pBuf = NULL;
    int bufSize = 1000;
    int curPos = 0;
    char *tmp = NULL;
    int incrSize;
    int curSize = bufSize;

    pBuf = (char *)anj_mw_malloc(bufSize);
    memset(pBuf, '\0', bufSize);

    char *pb = pBuf;
    char *pe = pBuf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<StorageFullAlarm\r\n");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pStorageFullAlm->enable);
    pb += snprintf(pb, pe - pb, "Threshold=\"%d\"\r\n", pStorageFullAlm->threshold);
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<AlarmAction>\r\n");

    curPos = pb - pBuf;
    tmp = anj_config_alarm_output_action_conver_xml(&(pStorageFullAlm->alarmAction.outputAction));
    if (tmp != NULL)
    {
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    /*
    pb += snprintf(pb, pe-pb, "<OSDDisplayAction\r\n");
    pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n", pStorageFullAlm->alarmAction.osdDisplayAction.enable);
    pb += snprintf(pb, pe-pb, "/>\r\n");
    */

    pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
    pb += snprintf(pb, pe - pb, "</StorageFullAlarm>\r\n");

    return pBuf;
}

char *anj_config_alarm_audio_conver_xml(AudioAlarm *pAlm)
{
    char *pBuf = NULL;
    int bufSize = 512;

    pBuf = (char *)anj_mw_malloc(bufSize);
    memset(pBuf, '\0', bufSize);

    char *pb = pBuf;
    char *pe = pBuf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<AudioAlarm\r\n");
    pb += snprintf(pb, pe - pb, "enable_babycry=\"%d\"\r\n", pAlm->enable_babycry);
    pb += snprintf(pb, pe - pb, "sensity_babycry=\"%d\"\r\n", pAlm->sensity_babycry);
    pb += snprintf(pb, pe - pb, "enable_lsd=\"%d\"\r\n", pAlm->enable_lsd);
    pb += snprintf(pb, pe - pb, "sensity_lsd=\"%d\"\r\n", pAlm->sensity_lsd);
    pb += snprintf(pb, pe - pb, "/>\r\n");

    return pBuf;
}

char *anj_config_alarm_video_gate_conver_xml(VideoGateAlarm *pAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        VideoGateAlarm *pAlm = &pAlmArray[cameraIndex];
        int enable_day = 0;
        int enable_night = 0;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_DAYTIME == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_day = 1;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_NIGHT == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_night = 1;

        pb += snprintf(pb, pe - pb, "<VideoGate ");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\" ", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\" >\r\n", pAlm->arming_flag);

        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            char szNodeName[32];
            sprintf(szNodeName, "Line%d", iIndex + 1);

            pb += snprintf(pb, pe - pb, "<%s ", szNodeName);
            pb += snprintf(pb, pe - pb, "fromx=\"%d\" ", pAlm->data[iIndex].x0Pos);
            pb += snprintf(pb, pe - pb, "fromy=\"%d\" ", pAlm->data[iIndex].y0Pos);
            pb += snprintf(pb, pe - pb, "tox=\"%d\" ", pAlm->data[iIndex].x1Pos);
            pb += snprintf(pb, pe - pb, "toy=\"%d\" ", pAlm->data[iIndex].y1Pos);
            pb += snprintf(pb, pe - pb, "EnableDay=\"%d\" ", enable_day);
            pb += snprintf(pb, pe - pb, "EnableNight=\"%d\" ", enable_night);
            pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", pAlm->data[iIndex].enable);
            pb += snprintf(pb, pe - pb, "sensitivity=\"%d\" ", pAlm->data[iIndex].sensitivity);
            pb += snprintf(pb, pe - pb, "type=\"%u\" ", pAlm->data[iIndex].type);
            pb += snprintf(pb, pe - pb, "direction=\"%d\" ", pAlm->data[iIndex].direction);

            pb += snprintf(pb, pe - pb, "/>\r\n");
        }

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb,
                       "<AlarmAction drawrect=\"%d\" draw_target=\"%d\" light_twinkle=\"%d\" notify_alarmserver=\"%d\" "
                       " alarm_led=\"%d\"  alarm_push=\"%d\" >\r\n",
                       pAlm->alarmAction.draw_rect_enable,
                       pAlm->alarmAction.draw_target_enable,
                       pAlm->alarmAction.light_twinkle_enable.enable_flag,
                       pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                       pAlm->alarmAction.alarm_led_enable.enable_flag,
                       pAlm->alarmAction.alarm_push.enable_flag);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</VideoGate>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_pd_conver_xml(PdAlarm *pAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        PdAlarm *pAlm = &pAlmArray[cameraIndex];
        int enable_day = 0;
        int enable_night = 0;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_DAYTIME == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_day = 1;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_NIGHT == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_night = 1;

        pb += snprintf(pb, pe - pb, "<VideoPD ");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\" ", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\" ", pAlm->arming_flag);
        pb += snprintf(pb, pe - pb, "EnableDay=\"%d\" ", enable_day);     // 对NVR/CLIENT前向兼容
        pb += snprintf(pb, pe - pb, "EnableNight=\"%d\" ", enable_night); // 对NVR/CLIENT前向兼容
        pb += snprintf(pb, pe - pb, "Type=\"%d\" ", pAlm->type);
        pb += snprintf(pb, pe - pb, "Threshold=\"%d\" ", pAlm->threshold);
        pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\" ", pAlm->sensitivity);
        pb += snprintf(pb, pe - pb, "minTargetRate=\"%d\" ", pAlm->minTargetRate);
        pb += snprintf(pb, pe - pb, "allowMd=\"%d\" ", pAlm->allowMd);
        pb += snprintf(pb, pe - pb, "nonMotionFilter=\"%d\" ", pAlm->nonMotionFilter);

        pb += snprintf(pb, pe - pb, "xPos=\"%d\" ", pAlm->area.xPos);
        pb += snprintf(pb, pe - pb, "yPos=\"%d\" ", pAlm->area.yPos);
        pb += snprintf(pb, pe - pb, "width=\"%d\" ", pAlm->area.width);
        pb += snprintf(pb, pe - pb, "height=\"%d\">\r\n", pAlm->area.height);

        curPos = pb - pBuf;
        tmp = anj_config_polygon_conver_xml(&(pAlm->polygonArea));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb,
                       "<AlarmAction drawrect=\"%d\" draw_human=\"%d\" track_human=\"%d\" auto_zoom=\"%d\" track_time=\"%d\" gunball_track_mode=\"%d\" "
                       "rect_twinkle=\"%d\"  light_twinkle=\"%d\" notify_alarmserver=\"%d\" alarm_led=\"%d\"  alarm_push=\"%d\" >\r\n",
                       pAlm->alarmAction.draw_rect_enable,
                       pAlm->alarmAction.draw_human_enable,
                       pAlm->alarmAction.track_human_enable,
                       pAlm->alarmAction.auto_zoom_enable,
                       pAlm->alarmAction.track_time,
                       pAlm->alarmAction.gunball_track_mode,
                       pAlm->alarmAction.rect_twinkle_enable,
                       pAlm->alarmAction.light_twinkle_enable.enable_flag,
                       pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                       pAlm->alarmAction.alarm_led_enable.enable_flag,
                       pAlm->alarmAction.alarm_push.enable_flag);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");

        pb += snprintf(pb, pe - pb, "<Sensitivitys>\r\n");
        unsigned int index = 0;
        for (index = 0; index < AI_TYPE_BIT_MAX; index++) // 尽量减少XML size
        {
            pb += snprintf(pb, pe - pb, "<d v=\"%u\" t=\"%u\" />\r\n", (unsigned int)pAlm->sensitivitys[index], index);
        }
        pb += snprintf(pb, pe - pb, "</Sensitivitys>\r\n");
        pb += snprintf(pb, pe - pb, "</VideoPD>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_region_ai_conver_xml(VideoRegionAiAlarm *pAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        VideoRegionAiAlarm *pAlm = &pAlmArray[cameraIndex];
        pb += snprintf(pb, pe - pb, "<VideoRegionAi ");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\" ", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\" >\r\n", pAlm->arming_flag);

        curPos = pb - pBuf;
        tmp = anj_config_polygon_conver_xml(&(pAlm->polygonArea));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        int iIndex = 0;
        for (iIndex = 0; iIndex < MAX_VIDEO_REGION_AI_NUM; iIndex++)
        {
            char szNodeName[32];
            sprintf(szNodeName, "Rule%d", iIndex + 1);

            pb += snprintf(pb, pe - pb, "<%s ", szNodeName);
            pb += snprintf(pb, pe - pb, "enable=\"%d\" ", pAlm->data[iIndex].enable);
            pb += snprintf(pb, pe - pb, "sensitivity=\"%d\" ", pAlm->data[iIndex].sensitivity);
            pb += snprintf(pb, pe - pb, "type_filter=\"%u\" ", pAlm->data[iIndex].type_filter);
            pb += snprintf(pb, pe - pb, "mode=\"%d\" ", pAlm->data[iIndex].mode);
            pb += snprintf(pb, pe - pb, "stayseconds=\"%d\" ", pAlm->data[iIndex].stayseconds);
            pb += snprintf(pb, pe - pb, "/>\r\n");
        }

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb,
                       "<AlarmAction drawrect=\"%d\" draw_target=\"%d\" light_twinkle=\"%d\" notify_alarmserver=\"%d\""
                       " alarm_led=\"%d\"  alarm_push=\"%d\" >\r\n",
                       pAlm->alarmAction.draw_rect_enable,
                       pAlm->alarmAction.draw_target_enable,
                       pAlm->alarmAction.light_twinkle_enable.enable_flag,
                       pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                       pAlm->alarmAction.alarm_led_enable.enable_flag,
                       pAlm->alarmAction.alarm_push.enable_flag);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</VideoRegionAi>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_sms_conver_xml(SMSAlarm *pAlm)
{
    char *pBuf = NULL;
    int initSize = 512;
    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;

    pb += snprintf(pb, pe - pb,
                   "<SMSAlarm enable=\"%d\" min_interval=\"%d\" playresult=\"%d\" dstnumber=\"%s\" content=\"%s\" />",
                   pAlm->enable, pAlm->min_interval, pAlm->playresult, pAlm->szSmsDstNum, pAlm->szSmsFixContent);

    return pBuf;
}

char *anj_config_alarm_lpr_conver_xml(LprAlarm *pAlmArray)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        LprAlarm *pAlm = &pAlmArray[cameraIndex];

        pb += snprintf(pb, pe - pb, "<Lpr ");
        pb += snprintf(pb, pe - pb, "camera=\"%d\"", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\"", pAlm->arming_flag);
        pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\"", pAlm->sensitivity);
        pb += snprintf(pb, pe - pb, "detectionmode=\"%d\"", pAlm->detectionmode);
        pb += snprintf(pb, pe - pb, "actionInterval=\"%d\"", pAlm->actionInterval);
        pb += snprintf(pb, pe - pb, "snapQuality=\"%s\"", pAlm->snapQuality);
        pb += snprintf(pb, pe - pb, " >\r\n");

        curPos = pb - pBuf;
        tmp = anj_config_polygon_conver_xml(&(pAlm->polygonArea));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb,
                       "<AlarmAction drawosd=\"%d\" playvoice=\"%d\" notify_alarmserver=\"%d\" alarm_push=\"%d\" "
                       " drawrect=\"%d\"  draw_target=\"%d\" >\r\n",
                       pAlm->alarmAction.draw_osd_enable,
                       pAlm->alarmAction.play_voice_enable,
                       pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                       pAlm->alarmAction.alarm_push.enable_flag,
                       pAlm->alarmAction.draw_rect_enable,
                       pAlm->alarmAction.draw_target_enable);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</Lpr>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_fire_conver_xml(FlameAndFlumesAlarm *pAlm)
{
    char *pBuf = NULL;
    int initSize = 1024;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;

    pb += snprintf(pb, pe - pb, "<FlameAndFlumes ");
    pb += snprintf(pb, pe - pb, "Enable=\"%d\" ", pAlm->enable);
    pb += snprintf(pb, pe - pb, "arming_flag=\"%d\" ", pAlm->arming_flag);
    pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\" ", pAlm->sensitivity);
    pb += snprintf(pb, pe - pb, "Sensitivity_smog=\"%d\" ", pAlm->sensitivity_smog);
    pb += snprintf(pb, pe - pb, "actionInterval=\"%d\" ", pAlm->actionInterval);
    pb += snprintf(pb, pe - pb, "snapQuality=\"%s\" ", pAlm->snapQuality);
    pb += snprintf(pb, pe - pb, " >\r\n");

#if 1
    curPos = pb - pBuf;
    tmp = anj_config_polygon_conver_xml(&(pAlm->polygonArea));
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);
#endif

    curPos = pb - pBuf;
    tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb,
                   "<AlarmAction drawrect=\"%d\" draw_target=\"%d\"  light_twinkle=\"%d\" notify_alarmserver=\"%d\" alarm_led=\"%d\"  alarm_push=\"%d\" >\r\n",
                   pAlm->alarmAction.draw_rect_enable,
                   pAlm->alarmAction.draw_target_enable,
                   pAlm->alarmAction.light_twinkle_enable.enable_flag,
                   pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                   pAlm->alarmAction.alarm_led_enable.enable_flag,
                   pAlm->alarmAction.alarm_push.enable_flag);

    if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
    {
        curPos = pb - pBuf;
        tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }
    if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
    {
        curPos = pb - pBuf;
        tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }
    if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
    {
        curPos = pb - pBuf;
        tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }
    if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
    {
        curPos = pb - pBuf;
        tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    curPos = pb - pBuf;
    tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
    if (tmp != NULL)
    {
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    curPos = pb - pBuf;
    tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
    if (tmp != NULL)
    {
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");

    pb += snprintf(pb, pe - pb, "</FlameAndFlumes>\r\n");

    return pBuf;
}

//--------------------------------------------//
// 这里面都是从配置文件中读取数据出来的
// config.xml
char *anj_config_alarm_temp_humidity_conver_xml(TempHumidityAlarm *pAlm)
{

    char *pBuf = NULL;
    int bufSize = 1024;
    char *tmp = NULL;
    int incrSize;
    int curSize = bufSize;
    int curPos = 0;
    //------//

    pBuf = (char *)anj_mw_malloc(bufSize);
    memset(pBuf, '\0', bufSize);

    char *pb = pBuf;
    char *pe = pBuf + bufSize - 1;

    pb += snprintf(pb, pe - pb, "<Temp_humidity\r\n");
    pb += snprintf(pb, pe - pb, "temp_enable=\"%d\"\r\n", pAlm->temp_enable);
    pb += snprintf(pb, pe - pb, "temp_upper_limit=\"%d\"\r\n", pAlm->temp_upper_limit);
    pb += snprintf(pb, pe - pb, "temp_lower_limit=\"%d\"\r\n", pAlm->temp_lower_limit);
    pb += snprintf(pb, pe - pb, "temp_range_lower=\"%d\"\r\n", pAlm->temp_range_lower);
    pb += snprintf(pb, pe - pb, "temp_range_upper=\"%d\"\r\n", pAlm->temp_range_upper);
    pb += snprintf(pb, pe - pb, "humidity_enable=\"%d\"\r\n", pAlm->humidity_enable);
    pb += snprintf(pb, pe - pb, "humidity_upper_limit=\"%d\"\r\n", pAlm->humidity_upper_limit);
    pb += snprintf(pb, pe - pb, "humidity_lower_limit=\"%d\"\r\n", pAlm->humidity_lower_limit);
    pb += snprintf(pb, pe - pb, "humidity_range_lower=\"%d\"\r\n", pAlm->humidity_range_lower);
    pb += snprintf(pb, pe - pb, "humidity_range_upper=\"%d\"\r\n", pAlm->humidity_range_upper);

    pb += snprintf(pb, pe - pb, "voc_enable=\"%d\"\r\n", pAlm->voc_enable);
    pb += snprintf(pb, pe - pb, "voc_good=\"%d\"\r\n", pAlm->voc_threashhold_good);
    pb += snprintf(pb, pe - pb, "voc_TracePollution=\"%d\"\r\n", pAlm->voc_threashhold_TracePollution);
    pb += snprintf(pb, pe - pb, "voc_LightPollution=\"%d\"\r\n", pAlm->voc_threashhold_LightPollution);
    pb += snprintf(pb, pe - pb, "voc_ModeratePollution=\"%d\"\r\n", pAlm->voc_threashhold_ModeratePollution);
    pb += snprintf(pb, pe - pb, "voc_HeavyPollution=\"%d\"\r\n", pAlm->voc_threashhold_HeavyPollution);

    // pb += snprintf(pb, pe-pb, "enable=\"%d\"\r\n",pAlm->enable);
    pb += snprintf(pb, pe - pb, ">\r\n");

    pb += snprintf(pb, pe - pb, "<AlarmAction>\r\n");

    curPos = pb - pBuf;
    tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
    if (tmp != NULL)
    {
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);
    }

    pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
    pb += snprintf(pb, pe - pb, "</Temp_humidity>\r\n");

    return pBuf;
}

char *anj_config_alarm_fd_conver_xml(FaceDetectAlarm *pAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        FaceDetectAlarm *pAlm = &pAlmArray[cameraIndex];
        int enable_day = 0;
        int enable_night = 0;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_DAYTIME == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_day = 1;
        if (ARMING_ALLDAY == pAlm->arming_flag || ARMING_NIGHT == pAlm->arming_flag || ARMING_CUSTOM == pAlm->arming_flag)
            enable_night = 1;

        pb += snprintf(pb, pe - pb, "<FaceDetect ");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\"", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\"", pAlm->arming_flag);
        pb += snprintf(pb, pe - pb, "EnableDay=\"%d\"", enable_day);
        pb += snprintf(pb, pe - pb, "EnableNight=\"%d\"", enable_night);
        pb += snprintf(pb, pe - pb, "Threshold=\"%d\"", pAlm->threshold);
        pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\"", pAlm->sensitivity);

        pb += snprintf(pb, pe - pb, "xPos=\"%d\"", pAlm->area.xPos);
        pb += snprintf(pb, pe - pb, "yPos=\"%d\"", pAlm->area.yPos);
        pb += snprintf(pb, pe - pb, "width=\"%d\"", pAlm->area.width);
        pb += snprintf(pb, pe - pb, "height=\"%d\">\r\n", pAlm->area.height);

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "<AlarmAction drawrect=\"%d\" >\r\n", pAlm->alarmAction.draw_rect_enable);

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</FaceDetect>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_video_cover_conver_xml(VideoCoverAlarm *pVideoCoverAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        VideoCoverAlarm *pVideoCoverAlm = &pVideoCoverAlmArray[cameraIndex];
        pb += snprintf(pb, pe - pb, "<VideoCoverAlarm\r\n");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\"\r\n", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pVideoCoverAlm->enable);
        pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\"\r\n", pVideoCoverAlm->sensitivity);
        pb += snprintf(pb, pe - pb, "threadhold_second=\"%d\"\r\n", pVideoCoverAlm->threadhold_second);
        pb += snprintf(pb, pe - pb, "backgroundUpdateSecond=\"%d\"\r\n", pVideoCoverAlm->backgroundUpdateSecond);
        pb += snprintf(pb, pe - pb, ">\r\n");

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pVideoCoverAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pVideoCoverAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "<AlarmAction>\r\n");

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pVideoCoverAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pVideoCoverAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</VideoCoverAlarm>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_video_lost_conver_xml(VideoLostAlarm *pVideoLostAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        VideoLostAlarm *pVideoLostAlm = &pVideoLostAlmArray[cameraIndex];
        pb += snprintf(pb, pe - pb, "<VideoLostAlarm\r\n");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\"\r\n", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pVideoLostAlm->enable);
        pb += snprintf(pb, pe - pb, ">\r\n");

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pVideoLostAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pVideoLostAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "<AlarmAction>\r\n");

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pVideoLostAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</VideoLostAlarm>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_motion_conver_xml(MotionDetectAlarm *pAlmArray, int camera_index, int bMsg)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;
    char escapeBuf[1000];

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && camera_index != cameraIndex)
        {
            continue;
        }

        MotionDetectAlarm *pAlm = &pAlmArray[cameraIndex];
        pb += snprintf(pb, pe - pb, "<MotionDetectAlarm\r\n");
        if (bMsg == 0)
            pb += snprintf(pb, pe - pb, "camera=\"%d\"\r\n", cameraIndex);
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pAlm->enable);
        pb += snprintf(pb, pe - pb, "arming_flag=\"%d\"\r\n", pAlm->arming_flag);
        pb += snprintf(pb, pe - pb, "BlockCount=\"%d\"\r\n", pAlm->blockCount);
        pb += snprintf(pb, pe - pb, "BlockConfig=\"%s\"\r\n", copy_with_escape(escapeBuf, pAlm->blockCfg));
        pb += snprintf(pb, pe - pb, "Sensitivity=\"%d\"\r\n", pAlm->sensitivity);
        pb += snprintf(pb, pe - pb, "AlarmThreshold=\"%d\"\r\n", pAlm->alarmThreshold);

        pb += snprintf(pb, pe - pb, "DayNightSwitch=\"%d\"\r\n", pAlm->dayNightSwitch);
        pb += snprintf(pb, pe - pb, "NightSensitivity=\"%d\"\r\n", pAlm->nightSensitivity);
        pb += snprintf(pb, pe - pb, "NightAlarmThreshold=\"%d\"\r\n", pAlm->nightAlarmThreshold);
        pb += snprintf(pb, pe - pb, "NightStartTime=\"%2d:%2d:%2d\"\r\n",
                       pAlm->nightTime.startTime.hour,
                       pAlm->nightTime.startTime.minute,
                       pAlm->nightTime.startTime.sec);
        pb += snprintf(pb, pe - pb, "NightEndTime=\"%2d:%2d:%2d\"\r\n",
                       pAlm->nightTime.endTime.hour,
                       pAlm->nightTime.endTime.minute,
                       pAlm->nightTime.endTime.sec);
        pb += snprintf(pb, pe - pb, ">\r\n");

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pAlm->timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pAlm->timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "<AlarmAction light_twinkle=\"%d\" notify_alarmserver=\"%d\"  alarm_led=\"%d\"  alarm_push=\"%d\" >\r\n",
                       pAlm->alarmAction.light_twinkle_enable.enable_flag,
                       pAlm->alarmAction.notify_alarmserver_enable.enable_flag,
                       pAlm->alarmAction.alarm_led_enable.enable_flag,
                       pAlm->alarmAction.alarm_push.enable_flag);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_led_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMLED, &(pAlm->alarmAction.alarm_led_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.alarm_push.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_ALARM_PUSH, &(pAlm->alarmAction.alarm_push));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pAlm->alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_ptz_action_conver_xml(&(pAlm->alarmAction.ptzAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pAlm->alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");

        pb += snprintf(pb, pe - pb, "</MotionDetectAlarm>\r\n");
    }

    return pBuf;
}

char *anj_config_alarm_input_conver_xml(InputAlarm *pInputAlm)
{
    char *pBuf = NULL;
    int initSize = 2000;
    char *tmp = NULL;
    int incrSize;
    int i;
    int curSize = initSize;
    int curPos = 0;
    char escapeBuf[1000];

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<InputAlarm>\r\n");

    for (i = 0; i < pInputAlm->channelCnt; i++)
    {
        pb += snprintf(pb, pe - pb, "<AlarmChannel\r\n");
        pb += snprintf(pb, pe - pb, "PortIndex=\"%d\"\r\n", pInputAlm->alarmChannels[i].portIndex);
        pb += snprintf(pb, pe - pb, "ChannelType=\"%s\"\r\n", copy_with_escape(escapeBuf, pInputAlm->alarmChannels[i].channelType.name));
        pb += snprintf(pb, pe - pb, "TriggerType=\"%s\"\r\n", copy_with_escape(escapeBuf, pInputAlm->alarmChannels[i].triggerType.name));
        pb += snprintf(pb, pe - pb, "Enable=\"%d\"\r\n", pInputAlm->alarmChannels[i].enable);
        pb += snprintf(pb, pe - pb, ">\r\n");

        TimeSpanList timeSpanList;
        TransTimeSpan2Old(&(pInputAlm->alarmChannels[i].timeSpan), &timeSpanList);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_list_conver_xml(&(timeSpanList));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        curSize = curSize + incrSize;
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curPos = curPos + strlen(tmp);
        pb = pBuf + curPos;
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        curPos = pb - pBuf;
        tmp = anj_config_timespan_conver_xml(&(pInputAlm->alarmChannels[i].timeSpan));
        incrSize = strlen(tmp);
        pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe - pb, "<AlarmAction light_twinkle=\"%d\" notify_alarmserver=\"%d\" notify_sms=\"%d\" >\r\n",
                       pInputAlm->alarmChannels[i].alarmAction.light_twinkle_enable.enable_flag,
                       pInputAlm->alarmChannels[i].alarmAction.notify_alarmserver_enable.enable_flag,
                       pInputAlm->alarmChannels[i].alarmAction.notify_sms.enable_flag);

        AlarmChannel *pAlm = &pInputAlm->alarmChannels[i];
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.light_twinkle_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_LIGHTWINKLE, &(pAlm->alarmAction.light_twinkle_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_alarmserver_enable.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_ALARMSERVER, &(pAlm->alarmAction.notify_alarmserver_enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }
        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->alarmAction.notify_sms.enable_flag == ARMING_CUSTOM)
        {
            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml(ARMING_TIMESPAN_XML_NODENAME_NOTIFY_SMS, &(pAlm->alarmAction.notify_sms));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_output_action_conver_xml(&(pInputAlm->alarmChannels[i].alarmAction.outputAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_ptz_action_conver_xml(&(pInputAlm->alarmChannels[i].alarmAction.ptzAction));

        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        curPos = pb - pBuf;
        tmp = anj_config_alarm_audio_action_conver_xml(&(pInputAlm->alarmChannels[i].alarmAction.audioAction));
        if (tmp != NULL)
        {
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);
        }

        /*
        curPos = pb - pBuf;
        tmp = MakeRecordActionXml(&(pInputAlm->alarmChannels[i].alarmAction.recordAction));
        incrSize = strlen(tmp);
        pBuf = anj_mw_realloc(pBuf, curSize + incrSize);
        memcpy(pBuf + curPos, tmp, strlen(tmp));
        curSize = curSize + incrSize;
        pb = pBuf + curPos + strlen(tmp);
        pe = pBuf + curSize - 1;
        anj_mw_free(tmp);

        pb += snprintf(pb, pe-pb, "<OSDDisplayAction\r\n");
        pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.osdDisplayAction.enable);
        pb += snprintf(pb, pe-pb, "/>\r\n");

        pb += snprintf(pb, pe-pb, "<PhotoTakeAction\r\n");
        pb += snprintf(pb, pe-pb, "Enable=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.photoTakeAction.enable);
        pb += snprintf(pb, pe-pb, "PreTakeTime=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.photoTakeAction.preTakeTime);
        pb += snprintf(pb, pe-pb, "TotalTakeTime=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.photoTakeAction.totalTakeTime);
        pb += snprintf(pb, pe-pb, "FtpUpload=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.photoTakeAction.ftpUpload);
        pb += snprintf(pb, pe-pb, "EmailUpload=\"%d\"\r\n", pInputAlm->alarmChannels[i].alarmAction.photoTakeAction.emailUpload);
        pb += snprintf(pb, pe-pb, "/>\r\n");
        */

        pb += snprintf(pb, pe - pb, "</AlarmAction>\r\n");
        pb += snprintf(pb, pe - pb, "</AlarmChannel>\r\n");
    }

    pb += snprintf(pb, pe - pb, "</InputAlarm>\r\n");

    return pBuf;
}

char *anj_config_alarm_output_conver_xml(OutPutAlarm *pOutputAlm)
{
    char *pBuf = NULL;
    int initSize = 2048;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pb = pBuf;
    char *pe = pBuf + initSize - 1;

    pb += snprintf(pb, pe - pb, "<OutputAlarm>\r\n");

    int i;
    for (i = 0; i < MAX_OUTPUT_CHANENL_COUNT; i++)
    {
        OutputChannel *pAlm = &pOutputAlm->outputChannels[i];
        int enable_day = 0;
        int enable_night = 0;
        if (ARMING_ALLDAY == pAlm->enable.enable_flag || ARMING_DAYTIME == pAlm->enable.enable_flag || ARMING_CUSTOM == pAlm->enable.enable_flag)
            enable_day = 1;
        if (ARMING_ALLDAY == pAlm->enable.enable_flag || ARMING_NIGHT == pAlm->enable.enable_flag || ARMING_CUSTOM == pAlm->enable.enable_flag)
            enable_night = 1;

        pb += snprintf(pb, pe - pb, "<OutputChannel\r\n");
        pb += snprintf(pb, pe - pb, "PortIndex=\"%d\"\r\n", pAlm->portIndex);
        pb += snprintf(pb, pe - pb, "ChannelType=\"%s\"\r\n", pAlm->channelType.name);
        pb += snprintf(pb, pe - pb, "TriggerType=\"%s\"\r\n", pAlm->triggerType.name);
        pb += snprintf(pb, pe - pb, "Duration=\"%d\"\r\n", pAlm->duration);

        pb += snprintf(pb, pe - pb, "enable_flag=\"%d\" ", pAlm->enable.enable_flag);
        pb += snprintf(pb, pe - pb, "DayEnable=\"%d\"\r\n", enable_day);
        pb += snprintf(pb, pe - pb, "NightEnable=\"%d\"\r\n", enable_night);
        // 下面2行用于兼容AjDevTools的字符串判断错误
        pb += snprintf(pb, pe - pb, "EnableDay=\"%d\"\r\n", enable_day);
        pb += snprintf(pb, pe - pb, "EnableNight=\"%d\"\r\n", enable_night);

        if (DEBUG_WRITE_ARMING_TIMESPAN || pAlm->enable.enable_flag == ARMING_CUSTOM)
        {
            pb += snprintf(pb, pe - pb, ">\r\n");

            curPos = pb - pBuf;
            tmp = anj_config_alarm_arming_daytimesapn_conver_xml("timespan", &(pAlm->enable));
            incrSize = strlen(tmp);
            pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
            memcpy(pBuf + curPos, tmp, strlen(tmp));
            curSize = curSize + incrSize;
            pb = pBuf + curPos + strlen(tmp);
            pe = pBuf + curSize - 1;
            anj_mw_free(tmp);

            pb += snprintf(pb, pe - pb, "</OutputChannel>\r\n");
        }
        else
        {
            pb += snprintf(pb, pe - pb, "/>\r\n");
        }
    }

    pb += snprintf(pb, pe - pb, "</OutputAlarm>\r\n");

    return pBuf;
}

char *anj_config_alarm_conver_xml(AlarmConfig *pAlarmCfg)
{
    char *pBuf = NULL;
    int initSize = 100;
    char *tmp = NULL;
    int incrSize;
    int curSize = initSize;
    int curPos = 0;

    pBuf = (char *)anj_mw_malloc(initSize);
    memset(pBuf, '\0', initSize);

    char *pe = pBuf + initSize - 1;
    char *pb = pBuf;

    pb += snprintf(pb, pe - pb, "<AlarmConfig>\r\n");

    curPos = pb - pBuf;
    tmp = anj_config_alarm_input_conver_xml(&pAlarmCfg->normalAlarm.inputAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_motion_conver_xml(pAlarmCfg->normalAlarm.motionDetectAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_video_lost_conver_xml(pAlarmCfg->normalAlarm.videoLostAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_video_cover_conver_xml(pAlarmCfg->normalAlarm.videoCoverAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_storage_full_conver_xml(&pAlarmCfg->normalAlarm.storageFullAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_audio_conver_xml(&pAlarmCfg->aiAlarm.audioAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_video_gate_conver_xml(pAlarmCfg->aiAlarm.vgAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_region_ai_conver_xml(pAlarmCfg->aiAlarm.regionAiAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_pd_conver_xml(pAlarmCfg->aiAlarm.pdAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_lpr_conver_xml(pAlarmCfg->aiAlarm.lprAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_fire_conver_xml(&pAlarmCfg->aiAlarm.fireAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_fd_conver_xml(pAlarmCfg->aiAlarm.fdAlarm, -1, 0);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_temp_humidity_conver_xml(&pAlarmCfg->normalAlarm.temphumidityAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_output_conver_xml(&pAlarmCfg->normalAlarm.outputAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    curPos = pb - pBuf;
    tmp = anj_config_alarm_sms_conver_xml(&pAlarmCfg->normalAlarm.smsAlarm);
    incrSize = strlen(tmp);
    pBuf = (char *)anj_mw_realloc(pBuf, curSize + incrSize);
    memcpy(pBuf + curPos, tmp, strlen(tmp));
    curSize = curSize + incrSize;
    pb = pBuf + curPos + strlen(tmp);
    pe = pBuf + curSize - 1;
    anj_mw_free(tmp);

    pb += snprintf(pb, pe - pb, "</AlarmConfig>\r\n");

    return pBuf;
}

int anj_config_alarm_get(IXML_Node *pNode, AlarmConfig *pAlarmCfg, int camera_index, int bMsg)
{
    memset(pAlarmCfg, 0, sizeof(AlarmConfig));
    for (int cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
    {
        if (camera_index >= 0 && cameraIndex != camera_index)
        {
            continue;
        }
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].enable = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].arming_flag = ARMING_ALLDAY;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].type = 16;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].minTargetRate = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].nonMotionFilter = 1;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].allowMd = SMART_MD_DEFAULT_ENABLE;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.draw_rect_enable = 1;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.draw_human_enable = 1;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.track_human_enable = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.rect_twinkle_enable = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].alarmAction.notify_alarmserver_enable.enable_flag = ARMING_DISABLE;

        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].area.xPos = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].area.yPos = 0;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].area.width = 100;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].area.height = 100;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].threshold = 75;
        pAlarmCfg->aiAlarm.pdAlarm[cameraIndex].sensitivity = 6;

        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].enable = 0;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].arming_flag = ARMING_ALLDAY;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].alarmAction.draw_rect_enable = 0;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].area.xPos = 0;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].area.yPos = 0;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].area.width = 100;
        pAlarmCfg->aiAlarm.fdAlarm[cameraIndex].area.height = 100;
    }

    pAlarmCfg->normalAlarm.temphumidityAlarm.temp_upper_limit = 70;
    pAlarmCfg->normalAlarm.temphumidityAlarm.temp_lower_limit = 0;
    pAlarmCfg->normalAlarm.temphumidityAlarm.humidity_upper_limit = 90;
    pAlarmCfg->normalAlarm.temphumidityAlarm.humidity_lower_limit = 15;
    pAlarmCfg->normalAlarm.temphumidityAlarm.humidity_range_lower = 0;
    pAlarmCfg->normalAlarm.temphumidityAlarm.humidity_range_upper = 100;

    pAlarmCfg->normalAlarm.temphumidityAlarm.temp_range_lower = -50;
    pAlarmCfg->normalAlarm.temphumidityAlarm.temp_range_upper = 100;

    pAlarmCfg->normalAlarm.temphumidityAlarm.temp_enable = 0;
    pAlarmCfg->normalAlarm.temphumidityAlarm.humidity_enable = 0;
    // pAlarmCfg->normalAlarm.temphumidityAlarm.enable = 1;

    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_enable = 0;
    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_threashhold_good = 500;
    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_threashhold_TracePollution = 1000;
    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_threashhold_LightPollution = 2000;
    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_threashhold_ModeratePollution = 3000;
    pAlarmCfg->normalAlarm.temphumidityAlarm.voc_threashhold_HeavyPollution = 5; // 用作告警等级

    pAlarmCfg->aiAlarm.audioAlarm.sensity_babycry = 50;
    pAlarmCfg->aiAlarm.audioAlarm.sensity_lsd = 50;

    IXML_Node *pChildNode = pNode->firstChild;

    while (pChildNode)
    {
        if (!strcmp(pChildNode->nodeName, "InputAlarm"))
        {
            anj_config_alarm_input_get(pChildNode, &pAlarmCfg->normalAlarm.inputAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "OutputAlarm"))
        {
            anj_config_alarm_output_get(pChildNode, &pAlarmCfg->normalAlarm.outputAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "MotionDetectAlarm"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_motion_get(pChildNode, &pAlarmCfg->normalAlarm.motionDetectAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "VideoLostAlarm"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_video_lost_get(pChildNode, &pAlarmCfg->normalAlarm.videoLostAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "VideoCoverAlarm"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_video_cover_get(pChildNode, &pAlarmCfg->normalAlarm.videoCoverAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "StorageFullAlarm"))
        {
            anj_config_alarm_storage_full_get(pChildNode, &pAlarmCfg->normalAlarm.storageFullAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "AudioAlarm"))
        {
            anj_config_alarm_audio_get(pChildNode, &pAlarmCfg->aiAlarm.audioAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "VideoGate"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_video_gate_get(pChildNode, &pAlarmCfg->aiAlarm.vgAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "VideoPD"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_pd_get(pChildNode, &pAlarmCfg->aiAlarm.pdAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "Lpr"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_lpr_get(pChildNode, &pAlarmCfg->aiAlarm.lprAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "FlameAndFlumes"))
        {
            anj_config_alarm_fire_get(pChildNode, &pAlarmCfg->aiAlarm.fireAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "FaceDetect"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_fd_get(pChildNode, &pAlarmCfg->aiAlarm.fdAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "Temp_humidity")) // Temp_humidity
        {
            anj_config_alarm_temp_humidity_get(pChildNode, &pAlarmCfg->normalAlarm.temphumidityAlarm);
        }
        else if (!strcmp(pChildNode->nodeName, "VideoRegionAi"))
        {
            int LoadCameraIndex = ANJ_CAMERA_MAX_NUMS;
            int camera = camera_index;
            anj_config_alarm_camera_get(pChildNode, &camera, &LoadCameraIndex);
            for (; camera < LoadCameraIndex; camera++)
            {
                anj_config_alarm_region_ai_get(pChildNode, &pAlarmCfg->aiAlarm.regionAiAlarm[camera], bMsg);
            }
        }
        else if (!strcmp(pChildNode->nodeName, "SMSAlarm"))
        {
            anj_config_alarm_sms_get(pChildNode, &pAlarmCfg->normalAlarm.smsAlarm);
        }
        pChildNode = pChildNode->nextSibling;
    }

    return 0;
}

int anj_config_alarm_save(AlarmConfig *pAlarmCfg)
{
    int iRet = 0;
    char *pDataXml = anj_config_alarm_conver_xml(pAlarmCfg);
    iRet = anj_config_save_node(pDataXml, "<AlarmConfig>", "</AlarmConfig>");
    anj_mw_free(pDataXml);
    return iRet;
}

int anj_config_alarm_set(AlarmConfig *pstAlarmConfig)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    if (memcmp(alarmCfg, pstAlarmConfig, sizeof(AlarmConfig)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(alarmCfg, pstAlarmConfig, sizeof(AlarmConfig));
        anj_config_alarm_save(pstAlarmConfig);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_fd_set(FaceDetectAlarm *pstFdAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pFdAlarmArray = alarmCfg->aiAlarm.fdAlarm;

    if (memcmp(pFdAlarmArray, pstFdAlarmArray, sizeof(FaceDetectAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        __WARN("Change!!!\n");
        memcpy(pFdAlarmArray, pstFdAlarmArray, sizeof(FaceDetectAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
        anj_smart_restart();
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_pd_set(PdAlarm *pstPdAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pPdAlarmArray = alarmCfg->aiAlarm.pdAlarm;

    if (memcmp(pPdAlarmArray, pstPdAlarmArray, sizeof(PdAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
        {
            anj_osd_polygon_update(&pstPdAlarmArray[i].polygonArea);
        }

        memcpy(pPdAlarmArray, pstPdAlarmArray, sizeof(PdAlarm) * ANJ_CAMERA_MAX_NUMS);
        __WARN("Change!!!\n");
        anj_config_alarm_save(alarmCfg);
        anj_smart_restart();
    }

    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_video_gate_set(VideoGateAlarm *pstVgAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pVgAlarmArray = alarmCfg->aiAlarm.vgAlarm;

    if (memcmp(pVgAlarmArray, pstVgAlarmArray, sizeof(VideoGateAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
        {
            anj_osd_cross_line_update(&pstVgAlarmArray[i]);
        }
        __WARN("Change!!!\n");
        memcpy(pVgAlarmArray, pstVgAlarmArray, sizeof(VideoGateAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
        anj_smart_restart();
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_region_set(VideoRegionAiAlarm *pstVrAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pVrAlarmArray = alarmCfg->aiAlarm.regionAiAlarm;

    if (memcmp(pVrAlarmArray, pstVrAlarmArray, sizeof(VideoRegionAiAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        // todo..
        __WARN("Change!!!\n");
        memcpy(pVrAlarmArray, pstVrAlarmArray, sizeof(VideoRegionAiAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_lpr_set(LprAlarm *pstLprAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    LprAlarm *pLprAlarmArray = alarmCfg->aiAlarm.lprAlarm;

    if (memcmp(pLprAlarmArray, pstLprAlarmArray, sizeof(LprAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        __WARN("Change!!!\n");
        memcpy(pLprAlarmArray, pstLprAlarmArray, sizeof(LprAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_fire_set(FlameAndFlumesAlarm *pstFireAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    FlameAndFlumesAlarm *pFireAlarm = &alarmCfg->aiAlarm.fireAlarm;

    if (memcmp(pFireAlarm, pstFireAlarmCfg, sizeof(FlameAndFlumesAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pFireAlarm, pstFireAlarmCfg, sizeof(FlameAndFlumesAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_audio_set(AudioAlarm *pstAudioAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    AudioAlarm *pAuAlarm = &alarmCfg->aiAlarm.audioAlarm;

    if (memcmp(pAuAlarm, pstAudioAlarmCfg, sizeof(AudioAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pAuAlarm, pstAudioAlarmCfg, sizeof(AudioAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_input_channel_set(AlarmChannel *pstAlarmChannel, int channel)
{
    if (channel < 0 && channel >= MAX_ALARMCHANNEL_COUNT)
    {
        return -1;
    }
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    AlarmChannel *pAlarmChannel = &alarmCfg->normalAlarm.inputAlarm.alarmChannels[channel];

    if (memcmp(pAlarmChannel, pstAlarmChannel, sizeof(AlarmChannel)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pAlarmChannel, pstAlarmChannel, sizeof(AlarmChannel));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_input_set(InputAlarm *pstInputAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    InputAlarm *pInputAlarm = &alarmCfg->normalAlarm.inputAlarm;

    if (memcmp(pInputAlarm, pstInputAlarmCfg, sizeof(InputAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pInputAlarm, pstInputAlarmCfg, sizeof(InputAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_motion_set(MotionDetectAlarm *pstMdAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pMdAlarmArray = alarmCfg->normalAlarm.motionDetectAlarm;

    if (memcmp(pMdAlarmArray, pstMdAlarmArray, sizeof(MotionDetectAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pMdAlarmArray, pstMdAlarmArray, sizeof(MotionDetectAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_video_lost_set(VideoLostAlarm *pstVideoLostAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoLostAlarm *pVlAlarmArray = alarmCfg->normalAlarm.videoLostAlarm;

    if (memcmp(pVlAlarmArray, pstVideoLostAlarmArray, sizeof(VideoLostAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pVlAlarmArray, pstVideoLostAlarmArray, sizeof(VideoLostAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_video_cover_set(VideoCoverAlarm *pstVideoCoverAlarmArray)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm *pVcAlarmArray = alarmCfg->normalAlarm.videoCoverAlarm;

    if (memcmp(pVcAlarmArray, pstVideoCoverAlarmArray, sizeof(VideoCoverAlarm) * ANJ_CAMERA_MAX_NUMS))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pVcAlarmArray, pstVideoCoverAlarmArray, sizeof(VideoCoverAlarm) * ANJ_CAMERA_MAX_NUMS);
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_storage_full_set(StorageFullAlarm *pstStoreFullAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    StorageFullAlarm *pStoreAlarm = &alarmCfg->normalAlarm.storageFullAlarm;

    if (memcmp(pStoreAlarm, pstStoreFullAlarmCfg, sizeof(StorageFullAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pStoreAlarm, pstStoreFullAlarmCfg, sizeof(StorageFullAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_output_channel_set(OutputChannel *pstOutputChannel, int channel)
{
    int changed = 0;
    OutputChannel stApply;

    if (channel < 0 || channel >= MAX_OUTPUT_CHANENL_COUNT)
    {
        return -1;
    }

    memset(&stApply, 0, sizeof(stApply));

    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    OutputChannel *pOutputChannel = &alarmCfg->normalAlarm.outputAlarm.outputChannels[channel];

    if (memcmp(pOutputChannel, pstOutputChannel, sizeof(OutputChannel)))
    {
        __WARN("Change!!!\n");
        memcpy(pOutputChannel, pstOutputChannel, sizeof(OutputChannel));
        anj_config_alarm_save(alarmCfg);
        memcpy(&stApply, pstOutputChannel, sizeof(OutputChannel));
        changed = 1;
    }
    pthread_rwlock_unlock(rwlock);

    if (changed)
    {
        anj_alarm_ioout_channel_apply(stApply.portIndex, stApply.triggerType.name);
    }

    return 0;
}

int anj_config_alarm_output_set(OutPutAlarm *pstOutputAlarmCfg)
{
    int i;
    int changed = 0;
    OutPutAlarm stApply;

    memset(&stApply, 0, sizeof(stApply));

    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    OutPutAlarm *pOutputAlarm = &alarmCfg->normalAlarm.outputAlarm;

    if (memcmp(pOutputAlarm, pstOutputAlarmCfg, sizeof(OutPutAlarm)))
    {
        __WARN("Change!!!\n");
        memcpy(pOutputAlarm, pstOutputAlarmCfg, sizeof(OutPutAlarm));
        anj_config_alarm_save(alarmCfg);
        memcpy(&stApply, pstOutputAlarmCfg, sizeof(OutPutAlarm));
        changed = 1;
    }
    pthread_rwlock_unlock(rwlock);

    if (changed)
    {
        for (i = 0; i < MAX_OUTPUT_CHANENL_COUNT; i++)
        {
            anj_alarm_ioout_channel_apply(
                stApply.outputChannels[i].portIndex,
                stApply.outputChannels[i].triggerType.name);
        }
    }

    return 0;
}

int anj_config_alarm_temphumidity_set(TempHumidityAlarm *pstTempAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    TempHumidityAlarm *pTempAlarm = &alarmCfg->normalAlarm.temphumidityAlarm;

    if (memcmp(pTempAlarm, pstTempAlarmCfg, sizeof(TempHumidityAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pTempAlarm, pstTempAlarmCfg, sizeof(TempHumidityAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_sms_set(SMSAlarm *pstSmsAlarmCfg)
{
    pthread_rwlock_t *rwlock = (pthread_rwlock_t *)getRWlock();

    pthread_rwlock_wrlock(rwlock);
    AlarmConfig *alarmCfg = (AlarmConfig *)getAlarmConfig();
    SMSAlarm *pSmsAlarm = &alarmCfg->normalAlarm.smsAlarm;

    if (memcmp(pSmsAlarm, pstSmsAlarmCfg, sizeof(SMSAlarm)))
    {
        // todo ...
        __WARN("Change!!!\n");
        memcpy(pSmsAlarm, pstSmsAlarmCfg, sizeof(SMSAlarm));
        anj_config_alarm_save(alarmCfg);
    }
    pthread_rwlock_unlock(rwlock);

    return 0;
}

int anj_config_alarm_read_lpr_config(AlarmOutputAction *p_ioOutput, AudioPlayAction *p_audioOutput)
{
    memset(p_ioOutput, 0, sizeof(AlarmOutputAction));
    p_ioOutput->channelCnt = 2;
    p_ioOutput->outputChnlActions[0].enable = 1;
    p_ioOutput->outputChnlActions[0].portIndex = 1;
    p_ioOutput->outputChnlActions[1].enable = 1;
    p_ioOutput->outputChnlActions[1].portIndex = 2;

    memset(p_audioOutput, 0, sizeof(AudioPlayAction));
    p_audioOutput->enable.enable_flag = ARMING_DISABLE;
    p_audioOutput->times = 1;

    char szConfigFile[256] = {0};
    sprintf(szConfigFile, "%s", "/opt/ch/lpr.action.xml");
    if (0 != access(szConfigFile, F_OK))
    {
        return 0;
    }

    char *pCfgXml = anj_mw_read_file_buffer(szConfigFile);
    IXML_Document *pDocNode = ixmlParseBuffer(pCfgXml);
    free(pCfgXml);

    if (pDocNode == NULL)
    {
        __ERR("ixmlParseBuffer error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "LPRAlarm");
    if (pNodelist != NULL)
    {
        IXML_Node *pNode = pNodelist->nodeItem;
        IXML_Node *tmpChild = pNode->firstChild;
        while (tmpChild)
        {
            if (!strcmp(tmpChild->nodeName, "AlarmAction"))
            {
                IXML_Node *tmp = tmpChild->firstChild;
                while (tmp)
                {
                    if (!strcmp(tmp->nodeName, "IOOutputAction"))
                    {
                        anj_config_alarm_output_action_get(tmp, p_ioOutput);
                    }
                    else if (!strcmp(tmp->nodeName, "AlarmOutputAction"))
                    {
                        anj_config_alarm_oldoutput_action_get(tmp, p_ioOutput);
                    }
                    else if (!strcmp(tmp->nodeName, "AudioPlayAction"))
                    {
                        anj_config_alarm_audio_action_get(tmp, p_audioOutput);
                    }

                    tmp = tmp->nextSibling;
                }
            }
            tmpChild = tmpChild->nextSibling;
        }

        ixmlNodeList_free(pNodelist);
        ixmlDocument_free(pDocNode);
        return 0;
    }
    else
    {
        __ERR("xmlDocument_getElementsByTagName(OEM_CONFIG) return NULL!\n");
        ixmlDocument_free(pDocNode);
        return -1;
    }
}

int anj_config_alarm_audio_get_by_xml(AudioAlarm *pAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AudioAlarm");
    if (pNodelist != NULL)
    {
        anj_config_alarm_audio_get(pNodelist->nodeItem, pAlm);
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

int anj_config_alarm_fd_get_by_xml(FaceDetectAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "FaceDetect");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_fd_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_fd_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_fd_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }
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

int anj_config_alarm_fire_get_by_xml(FlameAndFlumesAlarm *pAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "FlameAndFlumes");
    if (pNodelist != NULL)
    {
        anj_config_alarm_fire_get(pNodelist->nodeItem, pAlm);
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

int anj_config_alarm_input_get_by_xml(InputAlarm *pInputAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "InputAlarm");
    if (pNodelist != NULL)
    {
        anj_config_alarm_input_get(pNodelist->nodeItem, pInputAlm);
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

int anj_config_alarm_input_channel_get_by_xml(AlarmChannel *pAlarmChannel, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AlarmChannel");
    if (pNodelist != NULL)
    {
        anj_config_alarm_input_chn_get(pNodelist->nodeItem, pAlarmChannel);
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

int anj_config_alarm_lpr_get_by_xml(LprAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Lpr");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_lpr_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_lpr_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_lpr_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_motion_get_by_xml(MotionDetectAlarm *pMDAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MotionDetectAlarm");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_motion_get(pNodelist->nodeItem, &pMDAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_motion_get(pNodelist->nodeItem, &pMDAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_motion_get(pNodelist->nodeItem, &pMDAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_output_get_by_xml(OutPutAlarm *pOutputAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "OutputAlarm");
    if (pNodelist != NULL)
    {
        anj_config_alarm_output_get(pNodelist->nodeItem, pOutputAlm);
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

int anj_config_alarm_output_channel_get_by_xml(OutputChannel *pAlarmChannel, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "OutputChannel");
    if (pNodelist != NULL)
    {
        anj_config_alarm_output_chn_get(pNodelist->nodeItem, pAlarmChannel);
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

int anj_config_alarm_pd_get_by_xml(PdAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoPD");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_pd_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_pd_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_pd_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_region_get_by_xml(VideoRegionAiAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoRegionAi");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_region_ai_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_region_ai_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_region_ai_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_sms_get_by_xml(SMSAlarm *pAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "SMSAlarm");
    if (pNodelist != NULL)
    {
        anj_config_alarm_sms_get(pNodelist->nodeItem, pAlm);
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

int anj_config_alarm_storage_full_get_by_xml(StorageFullAlarm *pSFAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "StorageFullAlarm");
    if (pNodelist != NULL)
    {
        anj_config_alarm_storage_full_get(pNodelist->nodeItem, pSFAlm);
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

int anj_config_alarm_temp_humidity_get_by_xml(TempHumidityAlarm *pAlm, char *xmlBuf)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "Temp_humidity");
    if (pNodelist != NULL)
    {
        anj_config_alarm_temp_humidity_get(pNodelist->nodeItem, pAlm);
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

int anj_config_alarm_video_cover_get_by_xml(VideoCoverAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoCoverAlarm");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_video_cover_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_video_cover_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_video_cover_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_video_gate_get_by_xml(VideoGateAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoGate");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_video_gate_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_video_gate_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_video_gate_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_video_lost_get_by_xml(VideoLostAlarm *pAlmArray, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "VideoLostAlarm");
    if (pNodelist != NULL)
    {
        IXML_Node *tmpAttr = pNodelist->nodeItem->firstAttr;
        while (tmpAttr)
        {
            if (!strcmp(tmpAttr->nodeName, "camera"))
            {
                camera_index = Str2Num(tmpAttr->nodeValue);
                anj_config_alarm_video_lost_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
            }
            tmpAttr = tmpAttr->nextSibling;
        }
        if (camera_index < 0)
        {
            for (int i = 0; i < ANJ_CAMERA_MAX_NUMS; i++)
            {
                anj_config_alarm_video_lost_get(pNodelist->nodeItem, &pAlmArray[i], 1);
            }
        }
        else
        {
            anj_config_alarm_video_lost_get(pNodelist->nodeItem, &pAlmArray[camera_index], 1);
        }

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

int anj_config_alarm_get_by_xml(AlarmConfig *pAlarmCfg, char *xmlBuf, int camera_index)
{
    IXML_Document *pDocNode = ixmlParseBuffer(xmlBuf);

    if (pDocNode == NULL)
    {
        __ERR("xml error\r\n");
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "AlarmConfig");
    if (pNodelist != NULL)
    {
        anj_config_alarm_get(pNodelist->nodeItem, pAlarmCfg, camera_index, 1);
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
