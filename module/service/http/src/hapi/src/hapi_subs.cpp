#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <iostream>
#include <utility>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "anj_mw_comm.h"
#include "anj_mw_mutex.h"
#include "anj_mw_net.h"
#include "anj_mw_crypt.h"

#include "anj_config.h"
#include "anj_sysmng.h"
#include "anj_smart.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "alarm_link.h"
#include "cJSON.h"

#include "url_parse.h"
#include "hapi_subs.h"

#include <map>
#include <set>
#include <list>
#include <vector>

using namespace std;

#define TCP_SEND_TRY_TIMES 5
#define TCP_RETRY_RIGHTNOW 1

typedef struct
{
    int nAlarmType;
    int nAlarmSubType;
    char szAlarmType[64];
    char szAlarmSubType[64];
    int bEnd; // 强制设置报警结束标记（某些报警报的OCCUR，但是实际上是报警结束）
} AlarmTypeStrStruct;

static AlarmTypeStrStruct g_Defined_AlarmType[] =
{
    {ALARM_CODE_MOTION_DETECT, -1, "MotionDetect", "MotionAlarm", 0},                                              // 运动检测告警
    {ALARM_CODE_MOTION_DETECT_DISAPPEAR, -1, "MotionDetect", "MotionAlarm", 1},                                    // 运动检测告警恢复
    {ALARM_CODE_VIDEO_AI, ALARM_AI_PD, "ObjectDetect", "HumanShapeDetect", 0},                                     // 人形检测报警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_PD, "ObjectDetect", "HumanShapeDetect", 1},                              // 人形检测报警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_CAR, "ObjectDetect", "VehicleDetect", 0},                               // 车辆检测告警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VEHICLE_CAR, "ObjectDetect", "VehicleDetect", 1},                        // 车辆检测告警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_FALLINGOBJECT, "ObjectDetect", "FallingObjectsDetectionAlarm", 0},        // 高空抛物报警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_FALLINGOBJECT, "ObjectDetect", "FallingObjectsDetectionAlarm", 1}, // 高空抛物报警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VEHICLE_ELECTRICBICYCLE, "ObjectDetect", "ElectricbicycleDetect", 1},           // 电单车检测
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VEHICLE_ELECTRICBICYCLE, "ObjectDetect", "ElectricbicycleDetect", 1},    // 电单车检测
    {ALARM_CODE_VIDEO_AI, ALARM_AI_LPR, "LprDetect", "LprDetectAlarm", 0},                                         // 车牌识别报警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_LPR, "LprDetect", "LprDetectAlarm", 1},                                  // 车牌识别报警

    {ALARM_CODE_VIDEO_COVERD, -1, "MaskDetect", "MaskImageAlarm", 0}, // 遮挡侦测告警
    {-1, -1, "MaskDetect", "MaskImageAlarm", 1},                      // 遮挡侦测告警恢复

    {ALARM_CODE_AUDIO_BABYCRY, -1, "AudioDetect", "AbnormalAudio", 0},                                // 音频异常检测告警
    {ALARM_CODE_AUDIO_LSA, -1, "AudioDetect", "AbnormalAudio", 0},                                    // 音频异常检测告警
    {-1, -1, "AudioDetect", "AbnormalAudio", 0},                                                      // 音频异常检测告警恢复
    {ALARM_CODE_VIDEO_GATE, -1, "CrosslineDetect", "LineDetectorCrossed", 0},                         // 越界告警
    {ALARM_CODE_VIDEO_GATE_FINISH, -1, "CrosslineDetect", "LineDetectorCrossed", 1},                  // 越界告警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_GATE, "CrosslineDetect", "LineDetectorCrossed", 0},          // 越界告警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_GATE, "CrosslineDetect", "LineDetectorCrossed", 1},   // 越界告警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_GATE, "CrosslineDetect", "LineDetectorCrossed", 0},          // 越界告警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_GATE, "CrosslineDetect", "LineDetectorCrossed", 1},   // 越界告警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_ENTER, "RegionDetect", "EnterArea", 0},        // 进入区域
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_REGION_DETECT_ENTER, "RegionDetect", "EnterArea", 1}, // 进入区域
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_LEAVE, "RegionDetect", "LeaveArea", 0},        // 离开区域
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_REGION_DETECT_LEAVE, "RegionDetect", "LeaveArea", 1}, // 离开区域
    {ALARM_CODE_VIDEO_AI, ALARM_AI_VIDEO_REGION_DETECT_STAY, "RegionDetect", "Loitering", 0},         // 区域逗留
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_VIDEO_REGION_DETECT_STAY, "RegionDetect", "Loitering", 1},  // 区域逗留

    {-1, -1, "RegionDetect", "FenceCrossing", 0},                                                           // 翻越围栏
    {-1, -1, "RegionDetect", "ObjectRemoved", 0},                                                           // 物品看护
    {-1, -1, "RegionDetect", "ObjectLeftBehind", 0},                                                        // 物品遗留
    {-1, -1, "RegionDetect", "PeopleGathering", 0},                                                         // 人员聚集
    {-1, -1, "RegionDetect", "AreaPeopleCountingAlarm", 0},                                                 // 区域人数统计告警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_FIRE, "FireDetect", "FireAlarm", 0},                                     // 火焰告警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_FIRE, "FireDetect", "FireAlarm", 1},                              // 火焰告警
    {-1, -1, "FireDetect", "FireAlarm", 0},                                                                 // 火焰告警恢复
    {-1, -1, "FireDetect", "FumesAlarm", 0},                                                                // 烟雾告警
    {-1, -1, "FireDetect", "FumesAlarm", 0},                                                                // 烟雾告警结束
    {-1, -1, "FireDetect", "FlameAndFumesAlarm", 0},                                                        // 烟火告警
    {-1, -1, "FireDetect", "FlameAndFumesAlarm", 0},                                                        // 烟火告警结束
    {ALARM_CODE_VIDEO_AI, ALARM_AI_FACEDETECT, "FaceDetect", "FaceIsDetected", 0},                          // 人脸检测
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_FACEDETECT, "FaceDetect", "FaceIsDetected", 1},                   // 人脸检测
    {-1, -1, "FaceDetect", "FaceRecognition", 0},                                                           // 人脸比对报警
    {-1, -1, "FaceDetect", "FaceMatchAlarm", 0},                                                            // 人脸识别匹配报警
    {-1, -1, "FaceDetect", "FaceRecognitionMatchlistAlarm", 0},                                             // 人脸识别匹配报警
    {-1, -1, "FaceDetect", "FaceNotMatchAlarm", 0},                                                         // 人脸识别不匹配报警
    {-1, -1, "FaceDetect", "FaceRecognitionMismatchlistAlarm", 0},                                          // 人脸识别不匹配报警
    {ALARM_CODE_LPR, -1, "LprDetect", "LprDetectAlarm", 0},                                                 // 车牌识别报警
    {ALARM_CODE_VIDEO_AI, ALARM_AI_LPR, "LprDetect", "LprDetectAlarm", 0},                                  // 车牌识别报警
    {ALARM_CODE_VIDEO_AI_FINISH, ALARM_AI_LPR, "LprDetect", "LprDetectAlarm", 1},                           // 车牌识别报警
    {-1, -1, "LprDetect", "LprMatchlistAlarm", 0},                                                          // 车牌识别匹配报警
    {-1, -1, "LprDetect", "LprMismatchlistAlarm", 0},                                                       // 车牌识别不匹配报警
    {-1, -1, "LprDetect", "LprBlacklistAlarm", 0},                                                          // 停车场车辆识别黑名单报警
    {ALARM_CODE_ILLEGAL_MODIFY, -1, "IllegalLogin", "IllegalLogin", 0},                                     // 非法访问
    {-1, -1, "BatteryDetect", "LowBattery", 0},                                                             // 低电量报警
    {ALARM_CODE_LINKDOWN, -1, "NetworkDetect", "NetworkDisconnected", 0},                                   // 网络断开异常告警
    {ALARM_CODE_LINKUP, -1, "NetworkDetect", "NetworkDisconnected", 1},                                     // 网络断开恢复告警
    {ALARM_CODE_SAME_IP, -1, "NetworkDetect", "IPConflict", 0},                                             // IP冲突异常告警
    {-1, -1, "NetworkDetect", "IPConflict", 1},                                                             // IP地址冲突恢复
    {ALARM_CODE_IO_ALARM, -1, "IOInputDetect", "InputAlarm", 0},                                            // 输入开关量告警
    {ALARM_CODE_IO_ALARM_FINISH, -1, "IOInputDetect", "InputAlarm", 0},                                     // 输入开关量告警恢复
    {-1, -1, "TemperatureDetect", "TemperatureDetectionAlarm", 0},                                          // 温度检测事件
    {ALARM_CODE_TEMP_HUMID_ALARM, ALARM_AI_TEMP_UPPER, "TemperatureDetect", "HighTemperatureHighAlarm", 0}, // 温度过高报警
    {ALARM_CODE_TEMP_HUMID_ALARM, ALARM_AI_TEMP_LOWER, "TemperatureDetect", "TemperatureLowAlarm", 0},      // 温度过低报警
    {-1, -1, "TemperatureDetect", "TemperatureAbnormalAlarm", 0},                                           // 温度异常报警
    {-1, -1, "HumanStatusDetect", "SafetyHelmetAlarm", 0},                                                  // 未佩戴安全帽子报警
    {-1, -1, "HumanStatusDetect", "TelephoningAlarm", 0},                                                   // 打电话报警
    {-1, -1, "HumanStatusDetect", "SmokingAlarm", 0},                                                       // 吸烟告警
    {-1, -1, "HumanStatusDetect", "BodyTemperatureAlarm", 0},                                               // 体温异常告警
    {-1, -1, "HumanStatusDetect", "NoMaskAlarm", 0},                                                        // 未戴口罩告警
};

#define HAPI_ALARMSERVER_ID 0       // 报警服务器的上报集成到HAPI订阅中一起处理

static pthread_mutex_t s_stHapiSubMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_stHapiEventMutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_stHapiNotifyThread;

static map<unsigned int, HapiSubNodeStruct> s_HapiSubMap;       // 订阅管理map
static list<alarm_event_data> s_HapiEventList;                  // 报警事件列表

static int s_NotifyAlarmServerWithJpg = 0;

const char *pHapiPostNotifyHead =
    "POST /HAPI/V1.0/%sEvent/Notification HTTP/1.1\r\n"
    "User-Agent: HAPI NOTIFY\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: keep-alive\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "x-cos-meta-User-ID: %d\r\n"
    "Host: %s\r\n"
    "%s"
    "Content-Length: %d\r\n\r\n";

// BASIC认证: "username:password" 将字符串Base64编码为后，将其添加到请求头的 "Authorization" 字段中。

int hapi_notify_init();
int hapi_notify_uninit();


const AlarmTypeStrStruct *alarm_type_notify_get(const alarm_event_data *pAlarm)
{
    if (NULL == pAlarm)
        return NULL;

    unsigned int iIndex = 0;
    for (iIndex = 0; iIndex < sizeof(g_Defined_AlarmType) / sizeof(g_Defined_AlarmType[0]); iIndex++)
    {
        AlarmTypeStrStruct *p = &g_Defined_AlarmType[iIndex];
        if (p->nAlarmType == pAlarm->alarm_code)
        {
            if (p->nAlarmSubType > 0)
            {
                if (p->nAlarmSubType == pAlarm->alarm_level)
                {
                    return p;
                }
            }
            else
            {
                return p;
            }
        }
    }

    return NULL;
}

cJSON *json_motion_detect_get(const alarm_event_data *pAlarm, const AlarmTypeStrStruct *pAlarmTypeStr)
{
    cJSON *pEventInfoNode = cJSON_CreateObject();

    static char alarmdata[ALARM_MAX_PAYLOAD_LEN] = {0};
    static char buffer[ALARM_MAX_PAYLOAD_LEN / 2] = {0};

    cJSON *pAlarmDetails = NULL;
    int nBlockX = 0;
    int nBlockY = 0;

    int iRet = sscanf(pAlarm->alarm_payload, "motion alarm.%dX%d.%s", &nBlockY, &nBlockX, alarmdata);
    if (iRet == 3)
    {
        /*
        18X22表示移动侦测区域划分为18行22列。
        每一行22列占用3个byte共24个bit,共计3*18=54个bytes。把54个byte按16进制输出成字符串。
        每行的24个bit从第0开始到22有效，相应bit位为1表示相应行列的区块发生了Motion事件。
        */
        hexStrToUInt(alarmdata, strlen(alarmdata), (unsigned char*)buffer);

        if (nBlockX > MD_MAX_W_DIV_NUM)
            nBlockX = MD_MAX_W_DIV_NUM;
        if (nBlockY > MD_MAX_H_DIV_NUM)
            nBlockY = MD_MAX_H_DIV_NUM;

        pAlarmDetails = cJSON_CreateObject();
        cJSON_AddNumberToObject(pAlarmDetails, "motion_col", nBlockX);
        cJSON_AddNumberToObject(pAlarmDetails, "motion_row", nBlockY);

        cJSON *pMotionRowDetails = cJSON_CreateArray();

        int nBytesPerRow = ANJ_ALIGN_UP(nBlockX, 8) / 8;

        int yIndex = 0, xIndex = 0;
        for (yIndex = 0; yIndex < MD_MAX_H_DIV_NUM; yIndex++)
        {
            string szRowDetail = "";

            int fromBytes = nBytesPerRow * yIndex;
            for (xIndex = 0; xIndex < MD_MAX_W_DIV_NUM; xIndex++)
            {
                int bitspos = xIndex % 8;
                int bytespos = xIndex / 8;

                unsigned char value = GetBitValue(*(buffer + fromBytes + bytespos), bitspos);
                if (value == 0)
                    szRowDetail += "0";
                else
                    szRowDetail += "1";
            }

            cJSON *pNodeRow = cJSON_CreateObject();
            cJSON_AddStringToObject(pNodeRow, "motion_row", szRowDetail.c_str());
            cJSON_AddItemToArray(pMotionRowDetails, pNodeRow);
        }
        cJSON_AddItemToObject(pAlarmDetails, "motion_details", pMotionRowDetails);
    }

    // 图片
    cJSON *pImageInfo = NULL;
    if (NULL == pImageInfo)
        pImageInfo = cJSON_CreateNull();

    cJSON_AddItemToObject(pEventInfoNode, "image", pImageInfo);
    cJSON_AddItemToObject(pEventInfoNode, "MotionDetectInfo", pAlarmDetails);

    return pEventInfoNode;
}

cJSON *json_object_detect_get(const alarm_event_data *pAlarm, const AlarmTypeStrStruct *pAlarmTypeStr)
{
    cJSON *pEventInfoNode = cJSON_CreateObject();

    cJSON *pAlarmDetails = NULL;
    if (NULL == pAlarmDetails)
        pAlarmDetails = cJSON_CreateNull();

    // 图片
    cJSON *pImageInfo = NULL;
    if (NULL == pImageInfo)
        pImageInfo = cJSON_CreateNull();

    cJSON_AddItemToObject(pEventInfoNode, "image", pImageInfo);
    cJSON_AddItemToObject(pEventInfoNode, "ObjectDetectInfo", pAlarmDetails);

    return pEventInfoNode;
}

int hapi_alarm_need_notify_check(const alarm_event_data *pAlarm, const HapiSubNodeStruct &subNode)
{
    if (subNode.szEventType[0] == 0 || strcasestr(subNode.szEventType, "all") != NULL)
        return 1;

    int ret = 0;
    switch (pAlarm->alarm_code)
    {
    case ALARM_CODE_VIDEO_COVERD:
        if (strcasestr(subNode.szEventType, "MaskDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_MOTION_DETECT:
    case ALARM_CODE_MOTION_DETECT_DISAPPEAR:
        if (strcasestr(subNode.szEventType, "MotionDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_GPIO3_HIGH2LOW:
    case ALARM_CODE_GPIO3_LOW2HIGH:
    case ALARM_CODE_IO_ALARM:
    case ALARM_CODE_IO_ALARM_FINISH:
    case ALARM_CODE_EXTERNAL_IO_ALARM:
    case ALARM_CODE_EXTERNAL_IO_ALARM_FINISH:
        if (strcasestr(subNode.szEventType, "IOInputDetect") != NULL)
            ret = 1;
        break;

    case ALARM_CODE_VIDEO_AI:
    case ALARM_CODE_VIDEO_AI_FINISH:
    {
        switch (pAlarm->alarm_level)
        {
        case ALARM_AI_PD:                       // 1, //人形
        case ALARM_AI_VEHICLE_CAR:               // 2, //车形
        case ALARM_AI_VEHICLE_MOTO:               // 3, //摩托
        case ALARM_AI_VEHICLE_ELECTRICBICYCLE: // 4, //电单车
        case ALARM_AI_VEHICLE_BICYCLE:           // 5, //自行车
        {
            if (strcasestr(subNode.szEventType, "ObjectDetect") != NULL)
                ret = 1;

            break;
        }

        case ALARM_AI_LPR: // 6, //车牌
            if (strcasestr(subNode.szEventType, "LprDetect") != NULL)
                ret = 1;
            break;

        case ALARM_AI_VIDEO_GATE: // 7,//越界(拌线)
            if (strcasestr(subNode.szEventType, "CrosslineDetect") != NULL)
                ret = 1;
            break;

        case ALARM_AI_FIRE: // 8,    //火焰
            if (strcasestr(subNode.szEventType, "FireDetect") != NULL)
                ret = 1;
            break;
        case ALARM_AI_FACEDETECT: // 9,//FACE DETECT
            if (strcasestr(subNode.szEventType, "FaceDetect") != NULL)
                ret = 1;
            break;

        case ALARM_AI_VIDEO_REGION_DETECT_ENTER:
        case ALARM_AI_VIDEO_REGION_DETECT_LEAVE:
        case ALARM_AI_VIDEO_REGION_DETECT_STAY:
            if (strcasestr(subNode.szEventType, "RegionDetect") != NULL)
                ret = 1;
            break;

        default:
            break;
        }
    }
    break;
    case ALARM_CODE_LPR:
        if (strcasestr(subNode.szEventType, "LprDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_AUDIO_BABYCRY:
    case ALARM_CODE_AUDIO_LSA:
        if (strcasestr(subNode.szEventType, "AudioDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_VIDEO_GATE:
    case ALARM_CODE_VIDEO_GATE_FINISH:
        if (strcasestr(subNode.szEventType, "CrosslineDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_TEMP_HUMID_ALARM:
        if (strcasestr(subNode.szEventType, "TemperatureDetect") != NULL)
            ret = 1;
        break;
    case ALARM_CODE_ILLEGAL_MODIFY:
        if (strcasestr(subNode.szEventType, "IllegalLogin") != NULL)
            ret = 1;
        break;

    default:
        break;
    }

    return ret;
}

unsigned int hapi_submap_size_get()
{
    unsigned int nSize = s_HapiSubMap.size();
    return nSize;
}

// 检查订阅表的订阅时间
void hapi_submap_timeout_check()
{
    unsigned long long uNow = GetCurrentTimeStampU64();
    set<unsigned int> setToErase;

    anj_mutex_lock(&s_stHapiSubMutex);
    map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.begin();
    for (; it != s_HapiSubMap.end(); ++it)
    {
        unsigned int uid = it->first;
        HapiSubNodeStruct &data = it->second;

        if (HAPI_ALARMSERVER_ID == uid) // 配置的报警服务器节点，始终不超时
        {
            continue;
        }

        if (data.uptime_timeout < uNow)
        {
            setToErase.insert(uid);
            __INFO("hapi subscribe check %s:%d, ID %d timeout, delete it.\n", data.szServerName, data.nServerPort, uid);
        }
    }

    if (setToErase.size() > 0)      // 删除超时的ID
    {
        set<unsigned int>::iterator it2 = setToErase.begin();
        for (; it2 != setToErase.end(); ++it2)
        {
            unsigned int ID = *it2;
            s_HapiSubMap.erase(ID);
        }

        __DBG("Now hapi subscription size:%u\n", hapi_submap_size_get());
    }

    anj_mutex_unlock(&s_stHapiSubMutex);
}

unsigned int hapi_submap_add(HapiSubNodeStruct *pAddNode)
{
    // 处理新的订阅时删掉过期订阅，避免开启线程定时检查
    hapi_submap_timeout_check();

    static unsigned int uid = 0;
    unsigned int nFoundID = uid;

    anj_mutex_lock(&s_stHapiSubMutex);
    int bFound = 0;
    // 先找到相同IP/端口的订阅，避免重复订阅
    map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.begin();
    for (; it != s_HapiSubMap.end(); ++it)
    {
        HapiSubNodeStruct &stSubNode = it->second;

        if (stSubNode.nServerType == pAddNode->nServerType &&
            stSubNode.nServerPort == pAddNode->nServerPort &&
            strcmp(stSubNode.szServerName, pAddNode->szServerName) == 0)
        {
            nFoundID = it->first;

            memcpy(stSubNode.szPostURLPrefix, pAddNode->szPostURLPrefix, sizeof(stSubNode.szPostURLPrefix));
            memcpy(stSubNode.szEventType, pAddNode->szEventType, sizeof(stSubNode.szEventType));
            stSubNode.uptime_timeout = pAddNode->uptime_timeout;
            stSubNode.localtime_timeout = pAddNode->localtime_timeout;
            stSubNode.errortimes = 0;
            bFound = 1;

            __INFO("hapi subscribe add: %s:%d repeated for ID %u! post prefix:%s, event:%s. will timeout on uptime %llu!\n",
                     pAddNode->szServerName, pAddNode->nServerPort, nFoundID,
                     pAddNode->szPostURLPrefix, pAddNode->szEventType, pAddNode->uptime_timeout);
            break;
        }
    }

    if (0 == bFound)
    {
        uid++;
        while (uid < HAPI_ALARMSERVER_ID + 1) // 过滤掉报警服务器
        {
            uid++;
        }

        nFoundID = uid;
        s_HapiSubMap.insert(make_pair(nFoundID, *pAddNode));
    }
    anj_mutex_unlock(&s_stHapiSubMutex);

    __DBG("hapi subscribe add: %s:%d id:%u: post prefix:%s, event:%s. will timeout on uptime %llu! now size:%u\n",
             pAddNode->szServerName, pAddNode->nServerPort, nFoundID,
             pAddNode->szPostURLPrefix, pAddNode->szEventType, pAddNode->uptime_timeout,
             hapi_submap_size_get());

    hapi_notify_init();
    return nFoundID;
}

// 订阅刷新
int hapi_submap_refresh(unsigned int uid, unsigned int nDuration, HapiSubNodeStruct **pSubNode)
{
    int iRet = -1;

    anj_mutex_lock(&s_stHapiSubMutex);
    map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.find(uid);
    if (it != s_HapiSubMap.end())
    {
        HapiSubNodeStruct &data = it->second;
        data.localtime_timeout = time(0) + nDuration;
        data.uptime_timeout = GetCurrentTimeStampU64() + nDuration * 1000;
        data.errortimes = 0;
        __DBG("hapi subscribe refresh:%s:%d at %u seconds, id:%u\n", data.szServerName, data.nServerPort, nDuration, uid);

        iRet = 0;
        *pSubNode = &data;
    }
    else
    {
        __ERR("hapi subscribe refresh failed at %u seconds for id:%u\n", nDuration, uid);
        *pSubNode = NULL;
    }
    anj_mutex_unlock(&s_stHapiSubMutex);

    // 订阅刷新时删掉过期订阅，避免开启线程定时检查
    hapi_submap_timeout_check();

    return iRet;
}

// 订阅刷新
int hapi_submap_delete(unsigned int uid)
{
    int iRet = -1;

    anj_mutex_lock(&s_stHapiSubMutex);
    map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.find(uid);
    if (it != s_HapiSubMap.end())
    {
        HapiSubNodeStruct &data = it->second;
        __DBG("hapi subscribe delete: %s:%d for id:%u\n", data.szServerName, data.nServerPort, uid);
        s_HapiSubMap.erase(it);
        iRet = 0;
    }

    anj_mutex_unlock(&s_stHapiSubMutex);
    return iRet;
}

void hapi_alarm_event_notify(const alarm_event_data *pAlarm)
{
    if (NULL == pAlarm)
        return;

    anj_mutex_lock(&s_stHapiEventMutex);
    s_HapiEventList.push_back(*pAlarm);
    while (s_HapiEventList.size() > 5)
    {
        s_HapiEventList.pop_front();
    }
    anj_mutex_unlock(&s_stHapiEventMutex);

    return;    
}

static int hapi_connect_server(unsigned int nServerIp, int port, struct timeval tv)
{
    struct in_addr addr;
    memcpy(&addr.s_addr, &nServerIp, sizeof(unsigned int));

    if (tv.tv_sec == 0 && tv.tv_usec == 0)
    {
        tv.tv_usec = 500000;
    }
    else if (tv.tv_sec > 30)
    {
        tv.tv_sec = 30;
    }

    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    memcpy(&address.sin_addr.s_addr, &nServerIp, sizeof(unsigned int));
    if (address.sin_addr.s_addr == INADDR_NONE)
        return -1;

    int fd = -1;
    if (-1 == (fd = socket(PF_INET, SOCK_STREAM, 0)))
    {
        return -1;
    }

    int on = 0;
    // 禁止加强型nagle算法
    setsockopt(fd, SOL_TCP, TCP_CORK, (char *)&on, sizeof(on));
    // 禁止nagle算法
    on = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    // set the socket in non-blocking
    int flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (flags == -1)
    {
        __ERR("fcntl O_NONBLOCK failed with error: %d\n", flags);
    }

    int connected = connect(fd, (struct sockaddr *)&address, sizeof(address));
    if (connected != 0)
    {
        if (errno != EINPROGRESS)
        {
            __ERR("connect error :%s\n", strerror(errno));
            close(fd);
            return -1;
        }
    }

    fd_set fd_write;
    fd_set fd_err;
    FD_ZERO(&fd_write);
    FD_ZERO(&fd_err);
    FD_SET(fd, &fd_write);
    FD_SET(fd, &fd_err);

    // check if the socket is ready
    unsigned int t1 = GetCurrentTimeStamp();
    int iRet = select(fd + 1, NULL, &fd_write, &fd_err, &tv);
    unsigned int t2 = GetCurrentTimeStamp();
    __DBG("connect spend time: %ld to %ldms\n", t1, t2);

    if (iRet < 0)
    {
        __ERR("connect to server error, select failed: %s\n", strerror(errno));
    }
    else if (iRet == 0)
    {
        __DBG("connect to server timeout\n");
    }
    else if (iRet == 1)
    {
        if (FD_ISSET(fd, &fd_write))
        {
            __DBG("connect to server OK\n");
            // restart the socket mode
            flags = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
            if (flags == -1)
            {
                __ERR("fcntl ~O_NONBLOCK failed with error: %d\n", flags);
            }

            return fd;
        }
        else
        {
            __ERR("other error when select:%s\n", strerror(errno));
        }
    }

    close(fd);
    return -1;
}

static int hapi_send_to_server(int fd, const void *pBuffer, unsigned int nLen, int mode)
{
#if !TCP_RETRY_RIGHTNOW
    return send(fd, pBuffer, nLen, mode);
#else
    int iRet = 0;
    unsigned int sendsize = 0;
    unsigned int nTimes = 0;

    if (fd < 0)
        return -1;

    while (sendsize < nLen)
    {
        if (sendsize > 0)
        {
            // __ERR("total %d, sendsize %d.\n", nLen, sendsize);
            usleep(10 * 1000);
        }

        nTimes++;
        errno = 0;

        iRet = send(fd, (char *)pBuffer + sendsize, nLen - sendsize, mode);
        if (iRet <= 0)
        {
            if (errno == EAGAIN)
            {
                if (nTimes < TCP_SEND_TRY_TIMES)
                {
                    //__ERR("total %d, sendsize %d, ntimes %d.\n", nLen, sendsize, nTimes);
                    continue;
                }
            }

            if (nTimes > 1)
            {
                __ERR("total %d, sendsize %d, ntimes %d.\n", nLen, sendsize, nTimes);
            }

            if (sendsize == 0 && iRet < 0)
                return -1;
            else
                return sendsize;
        }

        sendsize = sendsize + iRet;
    }

    if (nTimes > 1 && sendsize < nLen)
    {
        __ERR("total %d, sendsize %d, ntimes %d.\n", nLen, sendsize, nTimes);
    }

    return sendsize;
#endif
}

int hapi_send_to_alarmserver(const char *msg, const HapiSubNodeStruct subsNode, const unsigned int uid)
{
    if (NULL == msg)
        return -1;

    unsigned int destIp = 0;
    if (subsNode.nServerType == SERVER_TYPE_DOMAINNAME)
    {
        destIp = WS_getIpFromName(subsNode.szServerName);
    }
    else
    {
        struct in_addr in;
        int r = inet_aton(subsNode.szServerName, &in);
        if (r != 0)
        {
            destIp = in.s_addr;
        }
    }

    if (destIp == 0)
    {
        __ERR("Get IP from server %s failed.\n", subsNode.szServerName);
        return -1;
    }

    struct timeval tv_timeout;
    tv_timeout.tv_sec = 0;
    tv_timeout.tv_usec = 500000;

    int fd = hapi_connect_server(destIp, subsNode.nServerPort, tv_timeout);
    if (fd < 0)
    {
        __ERR("connect %s:%d failed\n", subsNode.szServerName, subsNode.nServerPort);
        return -1;
    }

    int nMsgLength = 0;
    int nSendLen = 0;
    char *pHttpHeader = (char *)anj_mw_malloc(1024);
    if (NULL == pHttpHeader)
    {
        close(fd);
        __ERR("malloc failed for notify %s:%d\n", subsNode.szServerName, subsNode.nServerPort);
        return -1;
    }

    char szPostUrlPrefix[96] = {0};
    unsigned int nContentLength = strlen(msg);
    if (strlen(subsNode.szPostURLPrefix) > 0)
    {
        snprintf(szPostUrlPrefix, sizeof(szPostUrlPrefix), "%s/", subsNode.szPostURLPrefix);
    }

    char szAuthorization[512] = {0};
    if (strlen(subsNode.szUserName) > 0)
    {
        char szTmp[256] = {0};
        snprintf(szTmp, sizeof(szTmp), "%s:%s", subsNode.szUserName, subsNode.szPassword);

        int base64_len = Base64EncodeLen(szTmp);
        char *base64 = (char *)anj_mw_malloc(base64_len);
        if (base64 != NULL)
        {
            memset(base64, '\0', base64_len);
            Base64Encode((unsigned char *)szTmp, strlen(szTmp), base64);
            snprintf(szAuthorization, sizeof(szAuthorization), "Authorization: Basic %s\r\n", base64);

            anj_mw_free(base64);
        }
        else
        {
            __ERR("base64 malloc failed\n");
        }
    }

/*======== 发送 http header ========*/
    sprintf(pHttpHeader, pHapiPostNotifyHead, szPostUrlPrefix, uid, get_ip_addr_str_from_int(destIp), szAuthorization, nContentLength);
    nMsgLength = strlen(pHttpHeader);
    nSendLen = hapi_send_to_server(fd, pHttpHeader, nMsgLength, 0);

    anj_mw_free(pHttpHeader);
    pHttpHeader = NULL;

    if (nMsgLength != nSendLen)
    {
        close(fd);
        __ERR("send head error to %s:%d %d != %d.\n", subsNode.szServerName, subsNode.nServerPort, nSendLen, nMsgLength);
        return -1;
    }

/*======== 发送 http body ========*/
    nMsgLength = nContentLength;
    nSendLen = hapi_send_to_server(fd, msg, nMsgLength, 0);
    if (nMsgLength != nSendLen)
    {
        close(fd);
        __ERR("send body error to %s:%d %d != %d.\n", subsNode.szServerName, subsNode.nServerPort, nSendLen, nMsgLength);
        return -1;
    }

    // __DBG("Notify to %s:%d OK\n", subsNode.szServerName, subsNode.nServerPort);

    close(fd);
    return 0;
}


unsigned char *hapi_alarm_picture_get(int streamtype, int quality, unsigned long long *picturesize)
{
    unsigned char *JPGBuf = NULL;

    if (!picturesize)
    {
        return NULL;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);

    int Stream = streamtype;
    int Quality = quality * 10;

    char filename[128] = {0};
    const char *filepath = "/tmp";
    snprintf(filename, sizeof(filename), "alarmsnap_%d_%x_%x.jpg", Stream, (unsigned int)tv.tv_sec, (unsigned int)tv.tv_usec / 1000);
    anj_snap_jpg(0, Stream, Quality, (char *)filepath, filename, NULL);

    char filePath[160] = {0};
    snprintf(filePath, sizeof(filePath), "%s/%s", filepath, filename);
    if (0 == anj_snap_wait_complete(filePath, 5000))
    {
        anj_mw_read_file_len(filePath, picturesize);
        JPGBuf = (unsigned char *)anj_mw_malloc(*picturesize);
        anj_mw_read_file_limit_len(filePath, (char *)JPGBuf, *picturesize);

        remove(filePath);
    }
    else
    {
        __ERR("snap failed\n");
        goto RMEXIT;
    }

    return JPGBuf;

RMEXIT:

    if (JPGBuf)
        anj_mw_free(JPGBuf);

    *picturesize = 0;
    return NULL;
}

static void hapi_event_notify_all_sub_node(const alarm_event_data *pAlarmEventNode)
{
    if (NULL == pAlarmEventNode)
        return;

    if (hapi_submap_size_get() == 0)
    {
        return;
    }

    const AlarmTypeStrStruct *pAlarmTypeStr = alarm_type_notify_get(pAlarmEventNode);
    if (NULL == pAlarmTypeStr)
        return;

    char szLocalTime[128] = {0};
    struct timeval tv;
    struct tm *ptm, tbuf;
    gettimeofday(&tv, NULL);
    ptm = localtime_r(&tv.tv_sec, &tbuf);
    snprintf(szLocalTime, sizeof(szLocalTime), "%04d-%02d-%02d %02d:%02d:%02d",
            1900 + ptm->tm_year, 1 + ptm->tm_mon, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

    cJSON *pNotifyNode = cJSON_CreateObject();
    cJSON_AddStringToObject(pNotifyNode, "AlarmType", pAlarmTypeStr->szAlarmType);
    cJSON_AddStringToObject(pNotifyNode, "AlarmSubType", pAlarmTypeStr->szAlarmSubType);
    cJSON_AddNumberToObject(pNotifyNode, "TimeStamp", tv.tv_sec);
    cJSON_AddStringToObject(pNotifyNode, "LocalTime", szLocalTime);
    cJSON_AddNumberToObject(pNotifyNode, "ChannelNo", 0);
    cJSON_AddNumberToObject(pNotifyNode, "NotificationType", 0);

    if (pAlarmTypeStr->bEnd > 0) // 某些报警本身就是结束，直接写结束标记
    {
        cJSON_AddStringToObject(pNotifyNode, "OccurFlag", "false");
    }
    else // 没有配置的，按照报警消息中的alarmflag来判断。alarmflag高8位用来指示通道号，所以这里需要取低8位
    {
        int alarmflag = (pAlarmEventNode->alarm_flag & 0xff);
        cJSON_AddStringToObject(pNotifyNode, "OccurFlag", (ALARM_FLAG_DISAPPEAR != alarmflag) ? "true" : "false");
    }

    char mySN[32] = {0};
    anj_sysmng_load_sn(mySN, sizeof(mySN));
    cJSON_AddStringToObject(pNotifyNode, "DeviceID", mySN);

    cJSON *pAlarmInfo = NULL;
    switch (pAlarmEventNode->alarm_code)
    {
    case ALARM_CODE_MOTION_DETECT:
        pAlarmInfo = json_motion_detect_get(pAlarmEventNode, pAlarmTypeStr);
        break;

    case ALARM_CODE_VIDEO_AI:
        pAlarmInfo = json_object_detect_get(pAlarmEventNode, pAlarmTypeStr);
        break;

    default:
        break;
    }

/* ========= 组装图片buf开始 ============== */

    unsigned long long uFileSize = 0;
    unsigned char *JPGBuf = NULL;

    char szPath[128] = {0};
    if ((pAlarmEventNode->snap_path != NULL) && (strlen(pAlarmEventNode->snap_path) > 0))
    {
        cJSON *root = cJSON_Parse(pAlarmEventNode->snap_path);
        if (NULL != root)
        {
            cJSON *path = cJSON_GetObjectItem(root, "path");
            if (path != NULL)
            {
                snprintf(szPath, sizeof(szPath), "%s", path->valuestring);
            }
            cJSON_Delete(root);
            root = NULL;
        }
    }

    if (strlen(szPath) > 0)
    {
        if (anj_mw_file_exists(szPath))
        {
            __DBG("FileExist path->valuestring:%s\n", szPath);

            anj_mw_read_file_len(szPath, &uFileSize);
            if (uFileSize > 0)
            {
                JPGBuf = (unsigned char *)anj_mw_malloc(uFileSize);
                anj_mw_read_file_limit_len(szPath, (char *)JPGBuf, uFileSize);
            }
            else
            {
                __ERR("file:%s read len:%llu error!\n", uFileSize);
            }
        }
        else
        {
            __ERR(" NO FileExist path->valuestring:%s\n", szPath);
        }
    }
    else
    {
        if(s_NotifyAlarmServerWithJpg)
        {      
            JPGBuf = hapi_alarm_picture_get(1, 5, &uFileSize);
        }
    }

    __ERR("uFileSize:%lld\n", uFileSize);
    if (uFileSize > 0 && JPGBuf != NULL)
    {
        cJSON *Pic_date = cJSON_CreateObject();
        
        int len = (int)uFileSize;
        int base64_len = len / 3 * 4 + (len % 3 != 0) * 4 + 1;
        char *base64 = (char *)anj_mw_malloc(base64_len);
        memset(base64, '\0', sizeof(char) * base64_len);

        Base64Encode(JPGBuf, len, base64);

        cJSON_AddNumberToObject(Pic_date, "Size", base64_len);
        cJSON_AddStringToObject(Pic_date, "Data", base64);
        cJSON_AddItemToObject(pNotifyNode, "Picture", Pic_date);
        if (NULL != base64)
        {
            anj_mw_free(base64);
            base64 = NULL;
        }
    }

    if (NULL != JPGBuf)
    {
        anj_mw_free(JPGBuf);
        JPGBuf = NULL;
    }
/* ========= 组装图片buf结束 ============== */


    if (NULL == pAlarmInfo)
        pAlarmInfo = cJSON_CreateNull();
    cJSON_AddItemToObject(pNotifyNode, "AlarmInfo", pAlarmInfo);

    char *pJsonText = cJSON_PrintUnformatted(pNotifyNode);
    cJSON_Delete(pNotifyNode);

    int iRet = 0;
    if (NULL != pJsonText)
    {
        anj_mutex_lock(&s_stHapiSubMutex);
        map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.begin();
        for (; it != s_HapiSubMap.end(); ++it)
        {
            HapiSubNodeStruct &stSubNode = it->second;
            unsigned int uid = it->first;
            if( HAPI_ALARMSERVER_ID != uid )
            {
                if (stSubNode.errortimes > 120)
                {
                    if( HAPI_ALARMSERVER_ID == uid)
                    {
                        s_NotifyAlarmServerWithJpg = 0;
                    }
                    continue;
                }
            }

            if (hapi_alarm_need_notify_check(pAlarmEventNode, stSubNode) == 0)
            {
                continue;
            }

            iRet = hapi_send_to_alarmserver(pJsonText, stSubNode, uid);
            if (iRet < 0)
            {
                stSubNode.errortimes++;
                __ERR("Notify %s:%s to %s:%d failed.\n", pAlarmTypeStr->szAlarmType, pAlarmTypeStr->szAlarmSubType,
                         stSubNode.szServerName, stSubNode.nServerPort);
            }
            else
            {
                __DBG("Notify %s:%s to %s:%d OK.\n", pAlarmTypeStr->szAlarmType, pAlarmTypeStr->szAlarmSubType,
                         stSubNode.szServerName, stSubNode.nServerPort);
            }
        }
        anj_mutex_unlock(&s_stHapiSubMutex);

        if (pJsonText)
        {
            anj_mw_free(pJsonText);
            pJsonText = NULL;
        }
    }

    return;
}

static int hapi_notify_thread(void *ctx, int *bStart)
{
    while(bStart && *bStart)
    {
        anj_mutex_lock(&s_stHapiEventMutex);

        if (s_HapiEventList.size() > 0)
        {
            const alarm_event_data &stAlarmEventNode = s_HapiEventList.front();
            hapi_event_notify_all_sub_node(&stAlarmEventNode);
            s_HapiEventList.pop_front();
        }

        anj_mutex_unlock(&s_stHapiEventMutex);
        usleep(10 * 1000);
    }

    __INFO("hapi notify thread exit.");
    return 0;
}


int hapi_notify_init()
{
    int iRet = 0;
    if (s_stHapiNotifyThread.start == 1)
    {
        __INFO("hapi notify thread is running\n");
        return 0;
    }

    memset(&s_stHapiNotifyThread, 0, sizeof(anj_thread_s));
    s_stHapiNotifyThread.bAutoDestroy = 1;
    strncpy(s_stHapiNotifyThread.iThreadName, "hapi_notify_thread", sizeof(s_stHapiNotifyThread.iThreadName) - 1);
    s_stHapiNotifyThread.iThreadjob.ctx = &s_stHapiNotifyThread;
    s_stHapiNotifyThread.iThreadjob.func = hapi_notify_thread;
    iRet = anj_thread_task_create(&s_stHapiNotifyThread);
    return iRet;
}

int hapi_notify_uninit()
{
    if (s_stHapiNotifyThread.start == 1)
    {
        anj_thread_task_destroy(&s_stHapiNotifyThread, -1);
    }

    return 0;
}

void hapi_add_alarmserver()
{
    NetworkConfigNew *pstNetworkCfg = (NetworkConfigNew *)getNetWorkConfig();
    AlarmServerConfig *pstAlarmServerCfg = &pstNetworkCfg->alarmServerCfg;

    s_NotifyAlarmServerWithJpg = 0;

    if (strlen(pstAlarmServerCfg->url) > 0)
    {
        const char *p = (const char *)pstAlarmServerCfg->url;
        URL_RESULT_T result = {0};

        if (0 == parse_url(p, &result))
        {
            if (strlen(result.domain) > 0 && result.port > 0)
            {
                HapiSubNodeStruct stHapiSubNode;
                memset(&stHapiSubNode, 0, sizeof(HapiSubNodeStruct));

                if (check_is_ipv4(result.domain))
                {
                    stHapiSubNode.nServerType = SERVER_TYPE_IPV4;
                }
                else
                {
                    stHapiSubNode.nServerType = SERVER_TYPE_DOMAINNAME;
                }

                StrCpy(stHapiSubNode.szServerName, sizeof(stHapiSubNode.szServerName), result.domain);
                stHapiSubNode.nServerPort = result.port;
                strncpy(stHapiSubNode.szUserName, pstAlarmServerCfg->userName, sizeof(stHapiSubNode.szUserName) - 1);
                strncpy(stHapiSubNode.szPassword, pstAlarmServerCfg->password, sizeof(stHapiSubNode.szPassword) - 1);
                StrCpy(stHapiSubNode.szPostURLPrefix, sizeof(stHapiSubNode.szPostURLPrefix), result.svr_dir);

                int iDuration = 365 * (24 * 60 * 60);
                stHapiSubNode.localtime_timeout = time(0) + iDuration; // 一年后
                stHapiSubNode.uptime_timeout = GetCurrentTimeStamp() + iDuration * 1000;

                HapiSubNodeStruct *pstSubNode = &stHapiSubNode;
                unsigned int nFoundID = HAPI_ALARMSERVER_ID;

                anj_mutex_lock(&s_stHapiSubMutex);
                map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.find(nFoundID);
                if (it == s_HapiSubMap.end())
                {
                    s_HapiSubMap.insert(make_pair(nFoundID, stHapiSubNode));
                }
                else
                {
                    HapiSubNodeStruct &dst = it->second;
                    memcpy(&dst, &stHapiSubNode, sizeof(HapiSubNodeStruct));
                }
                anj_mutex_unlock(&s_stHapiSubMutex);

                __INFO("hapi:%s:%d subscription %u: Post prefix: %s, event %s. will timeout on uptime %llu!\n",
                    pstSubNode->szServerName, 
                    pstSubNode->nServerPort, nFoundID,
                    pstSubNode->szPostURLPrefix, 
                    pstSubNode->szEventType, 
                    pstSubNode->uptime_timeout);

                __INFO("hapi now subscription size=%u\n", hapi_submap_size_get());

                s_NotifyAlarmServerWithJpg = pstAlarmServerCfg->withattachment;

                hapi_notify_init();
            }
        }
        else
        {
            __ERR("hapi parse failed: %s\n", p);
        }
    }
    else
    {
        unsigned int nFoundID = HAPI_ALARMSERVER_ID;
        anj_mutex_lock(&s_stHapiSubMutex);
        map<unsigned int, HapiSubNodeStruct>::iterator it = s_HapiSubMap.find(nFoundID);
        if (it != s_HapiSubMap.end())
        {
            s_HapiSubMap.erase(it);
        }
        anj_mutex_unlock(&s_stHapiSubMutex);
    }
}

void hapi_destory_alarmserver()
{
    hapi_notify_uninit();    
}

