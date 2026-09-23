#include <ctype.h>
#include <stdio.h>   
#include <iostream>  
#include <utility>   
#include <stdlib.h>  
#include <sys/stat.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>  
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "anj_mw_comm.h"
#include "anj_mw_net.h"
#include "anj_mw_crypt.h"

#include "anj_audio.h"
#include "anj_video.h"
#include "anj_snap.h"
#include "anj_config.h"
#include "cJSON.h"
#include "alarm_link.h"

#include "http_handle.h"
#include "unv_subs.h"

using namespace std;
#include <map>
#include <set>

static pthread_mutex_t s_stUnvSubMutex = PTHREAD_MUTEX_INITIALIZER;
static map<unsigned int, Unv_Sub_Node>s_UnvSubMap;

static char unv_str_subscribe_head[] = 
"    \"Reference\":    \"%s:%d/Subscription/Subscribers/%d\",\r\n";

static char unv_str_structure_head[] = 
"POST /LAPI/V1.0/System/Event/Notification/Structure HTTP/1.1\r\n\
Host: %s:%d\r\n\
Accept: */*\r\n\
cache-control: no-cache\r\n\
content-type: multipart/form-data\r\n\
Content-Length: %d\r\n\
Expect: 100-continue\r\n\r\n";

static char unv_str_alarm_head[] = 
"POST /LAPI/V1.0/System/Event/Notification/Alarm HTTP/1.1\r\n\
Host: %s:%d\r\n\
Accept: */*\r\n\
cache-control: no-cache\r\n\
content-type: multipart/form-data\r\n\
Content-Length: %d\r\n\
Expect: 100-continue\r\n\r\n";


static int32_t os_socket_connect(int32_t skt, char *strip, uint16_t port, int32_t timeout)
{
    struct sockaddr_in addr;
    uint32_t unblock = 1;
    fd_set rset;
    struct timeval tv;
    int32_t len = 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(strip);
    addr.sin_port = htons(port);

    len = sizeof(struct sockaddr);

    if (timeout < 0)
    {
        return connect(skt, (struct sockaddr *)&addr, len);
    }

    // 设置为非阻塞状态
    if (ioctl(skt, FIONBIO, &unblock) < 0)
    {
        return -1;
    }

    // 开始连接
    connect(skt, (struct sockaddr *)&addr, sizeof(addr));

    // 等待连接返回
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    FD_ZERO(&rset);
    FD_SET((uint32_t)skt, &rset);
    
    if (select(skt + 1, 0, &rset, 0, &tv) <= 0)
    {
        return -1;
    }

    unblock = 0;
    // 设回阻塞模式
    return ioctl(skt, FIONBIO, &unblock);
}

static int32_t os_socket_send(int32_t skt, char *buf, int32_t size, int32_t timeout)
{
    int ret = 0;
    int sendlen = 0;
    struct timeval tv;
    fd_set wset;

    if (timeout < 0)
    {
        return send(skt, buf, size, 0);
    }

    while (sendlen < size)
    {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        
        FD_ZERO(&wset);
        FD_SET((uint32_t)skt, &wset);

        if (select(skt + 1, 0, &wset, 0, &tv) <= 0)
        {
            return -1;
        }

        ret = send(skt, buf + sendlen, size - sendlen, MSG_DONTWAIT);
        if (ret <= 0)
        {            
            if (ret < 0 && errno == EINTR)
            {
                continue;
            }

            if (ret < 0 && errno == EAGAIN)
            {
                continue;
            }

            if (ret == 0)
            {
                return 0;
            }
            
            return -1;
        }

        sendlen += ret;
    }

    return sendlen;
}

int alarm_objinfo_rule_get(alarm_event_data *p_alarm_event, cJSON **p)
{
    cJSON *RuleInfoJson = NULL;
    RuleInfoJson = cJSON_CreateObject();
    if (RuleInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject RuleInfoJson failed\n");
        return -1;
    }

    if (p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_STAY)
    {
        cJSON_AddNumberToObject(RuleInfoJson, "RuleType", 0);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_VIDEO_GATE)
    {
        cJSON_AddNumberToObject(RuleInfoJson, "RuleType", 1);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_LEAVE)
    {
        cJSON_AddNumberToObject(RuleInfoJson, "RuleType", 2);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_ENTER)
    {
        cJSON_AddNumberToObject(RuleInfoJson, "RuleType", 3);
    }
    else
    {
        cJSON_AddNumberToObject(RuleInfoJson, "RuleType", 5);
    }

    cJSON_AddNumberToObject(RuleInfoJson, "TrigerType", 0);
    cJSON_AddNumberToObject(RuleInfoJson, "PointNum", 0);

    *p = RuleInfoJson;
    return 0;
}

int alarm_objinfo_person_get(alarm_event_data *p_alarm_event, cJSON **p)
{
    cJSON *RuleInfoJson = NULL;
    alarm_objinfo_rule_get(p_alarm_event, &RuleInfoJson);

    cJSON *PersonInfoJson = cJSON_CreateObject();
    if (PersonInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject PersonInfoJson failed\n");
        return -1;
    }

    cJSON_AddNumberToObject(PersonInfoJson, "PersonID", 1);
    cJSON_AddStringToObject(PersonInfoJson, "Position", "0,0;10000,10000");
    cJSON_AddNumberToObject(PersonInfoJson, "SmallPicAttachIndex", 2);
    cJSON_AddNumberToObject(PersonInfoJson, "LargePicAttachIndex", 1);

    cJSON_AddItemToObject(PersonInfoJson, "RuleInfo", RuleInfoJson);

    *p = PersonInfoJson;
    return 0;
}

int alarm_objinfo_vehicle_get(alarm_event_data *p_alarm_event, cJSON **p)
{
    cJSON *RuleInfoJson = NULL;
    alarm_objinfo_rule_get(p_alarm_event, &RuleInfoJson);
    
    cJSON *VehicleInfoJson = cJSON_CreateObject();
    if (VehicleInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject VehicleInfoJson failed\n");
        return -1;
    }

    cJSON_AddNumberToObject(VehicleInfoJson, "ID", 1);
    cJSON_AddStringToObject(VehicleInfoJson, "Position", "0,0;10000,10000");
    cJSON_AddNumberToObject(VehicleInfoJson, "SmallPicAttachIndex", 2);
    cJSON_AddNumberToObject(VehicleInfoJson, "LargePicAttachIndex", 1);

    if (RuleInfoJson != NULL)
    {
        cJSON_AddItemToObject(VehicleInfoJson, "RuleInfo", RuleInfoJson);
    }

    *p = VehicleInfoJson;
    return 0;
}

int alarm_objinfo_non_motor_vehicle_get(alarm_event_data *p_alarm_event, cJSON **p)
{
    cJSON *RuleInfoJson = NULL;
    alarm_objinfo_rule_get(p_alarm_event, &RuleInfoJson);

    cJSON *NonMotorVehicleInfoJson = cJSON_CreateObject();
    if (NonMotorVehicleInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject NonMotorVehicleInfoJson failed\n");
        return -1;
    }

    cJSON *AttributeInfoJson = cJSON_CreateObject();
    if (AttributeInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject AttributeInfoJson failed\n");
        cJSON_Delete(NonMotorVehicleInfoJson);
        return -1;
    }

    cJSON_AddNumberToObject(NonMotorVehicleInfoJson, "ID", 1);
    cJSON_AddStringToObject(NonMotorVehicleInfoJson, "Position", "0,0;10000,10000");
    cJSON_AddNumberToObject(NonMotorVehicleInfoJson, "SmallPicAttachIndex", 2);
    cJSON_AddNumberToObject(NonMotorVehicleInfoJson, "LargePicAttachIndex", 1);

    cJSON_AddNumberToObject(AttributeInfoJson, "SpeedType", 0);
    cJSON_AddNumberToObject(AttributeInfoJson, "ImageDirection", 0);
    cJSON_AddNumberToObject(AttributeInfoJson, "NonVehicleType", 0);

    cJSON_AddItemToObject(NonMotorVehicleInfoJson, "AttributeInfo", AttributeInfoJson);
    
    if (RuleInfoJson != NULL)
    {
        cJSON_AddItemToObject(NonMotorVehicleInfoJson, "RuleInfo", RuleInfoJson);
    }
    
    *p = NonMotorVehicleInfoJson;
    return 0;
}

int alarm_objinfo_face_get(alarm_event_data *p_alarm_event, cJSON **p, int id)
{
    cJSON *FaceInfoJson = cJSON_CreateObject();
    if (FaceInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject FaceInfoJson failed\n");
        return -1;
    }

    cJSON *AttributeInfoJson = cJSON_CreateObject();
    if (AttributeInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject AttributeInfo failed\n");
        cJSON_Delete(FaceInfoJson);
        return -1;
    }

    cJSON_AddNumberToObject(FaceInfoJson, "FaceID", id + 1);
    cJSON_AddNumberToObject(FaceInfoJson, "FaceDoforPersonID", 0);
    
    if (p_alarm_event->snap_path != NULL && (strlen(p_alarm_event->snap_path) > 0))
    {
        cJSON *root = cJSON_Parse(p_alarm_event->snap_path);
        if (root != NULL)
        {
            cJSON *pos = cJSON_GetObjectItem(root, "pos");
            if (pos != NULL)
            {
                cJSON *p_t = cJSON_GetArrayItem(pos, id);
                if (p_t != NULL)
                {
                    char pos_msg[64] = {0};

                    cJSON *x = cJSON_GetObjectItem(p_t, "x");
                    cJSON *y = cJSON_GetObjectItem(p_t, "y");
                    cJSON *w = cJSON_GetObjectItem(p_t, "w");
                    cJSON *h = cJSON_GetObjectItem(p_t, "h");
                    
                    __ERR("x->valueint:%d\n", x->valueint);
                    __ERR("y->valueint:%d\n", y->valueint);
                    __ERR("w->valueint:%d\n", w->valueint);
                    __ERR("h->valueint:%d\n", h->valueint);
                    
                    if (x != NULL && y != NULL && w != NULL && h != NULL &&
                        x->valueint >= 0 && x->valueint <= 10000 && 
                        y->valueint >= 0 && y->valueint <= 10000 && 
                        w->valueint >= 0 && w->valueint <= 10000 && 
                        h->valueint >= 0 && h->valueint <= 10000 )
                    {
                        snprintf(pos_msg, sizeof(pos_msg), "%d,%d;%d,%d", x->valueint, y->valueint, w->valueint, h->valueint);
                        cJSON_AddStringToObject(FaceInfoJson, "Position", pos_msg);
                    }
                    else
                    {
                        cJSON_AddStringToObject(FaceInfoJson, "Position", "0,0;10000,10000");
                    }
                }
            }
            cJSON_Delete(root);
        }
    }
    else
    {
        cJSON_AddStringToObject(FaceInfoJson, "Position", "0,0;10000,10000");
    }
    
    cJSON_AddNumberToObject(FaceInfoJson, "SmallPicAttachIndex", 2);
    cJSON_AddNumberToObject(FaceInfoJson, "LargePicAttachIndex", 1);
    cJSON_AddStringToObject(FaceInfoJson, "FeatureVersion", " ");
    cJSON_AddStringToObject(FaceInfoJson, "Feature", " ");

    cJSON_AddNumberToObject(AttributeInfoJson, "Gender", 0);
    cJSON_AddNumberToObject(AttributeInfoJson, "AgeRange", 0);
    cJSON_AddNumberToObject(AttributeInfoJson, "GlassFlag", 0);
    cJSON_AddNumberToObject(AttributeInfoJson, "GlassesStyle", 0);
    
    cJSON_AddItemToObject(FaceInfoJson, "AttributeInfo", AttributeInfoJson);
    
    *p = FaceInfoJson;
    return 0;
}

int alarm_objinfo_get(alarm_event_data *p_alarm_event, cJSON **pJson)
{
    cJSON *ObjInfo = cJSON_CreateObject();
    if (ObjInfo == NULL)
    {
        __ERR("cJSON_CreateObject ObjInfo failed\n");
        return -1;
    }

    cJSON *pFaceInfoJson = NULL;
    cJSON *pFaceInfoListJson = NULL;
    if (0)
    {
        ;
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_FACEDETECT)
    {
        cJSON_AddNumberToObject(ObjInfo, "FaceNum", 1);

        pFaceInfoListJson = cJSON_CreateArray();
        if (pFaceInfoListJson == NULL)
        {
            __ERR("cJSON_CreateArray failed\n");
            cJSON_Delete(ObjInfo);
            return -1;
        }

        alarm_objinfo_face_get(p_alarm_event, &pFaceInfoJson, 0);
        cJSON_AddItemToArray(pFaceInfoListJson, pFaceInfoJson);
        cJSON_AddItemToObject(ObjInfo, "FaceInfoList", pFaceInfoListJson);
    }
    else
    {
        cJSON_AddNumberToObject(ObjInfo, "FaceNum", 0);

        pFaceInfoListJson = cJSON_CreateArray();
        cJSON_AddItemToObject(ObjInfo, "FaceInfoList", pFaceInfoListJson);
    }

    cJSON *pPersonInfoJson = NULL;
    cJSON *pPersonInfoListJson = NULL;
    cJSON *pNonMotorVehicleInfoJson = NULL;
    cJSON *pNonMotorVehicleInfoListJson = NULL;
    cJSON *pVehicleInfoJson = NULL;
    cJSON *pVehicleInfoListJson = NULL;

    if (p_alarm_event->alarm_level == ALARM_AI_PD ||
        p_alarm_event->alarm_level == ALARM_AI_VIDEO_GATE ||
        p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_ENTER || 
        p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_LEAVE ||
        p_alarm_event->alarm_level == ALARM_AI_VIDEO_REGION_DETECT_STAY )
    {
        pPersonInfoJson = NULL;
        pPersonInfoListJson = cJSON_CreateArray();
        alarm_objinfo_person_get(p_alarm_event, &pPersonInfoJson);
        cJSON_AddItemToArray(pPersonInfoListJson, pPersonInfoJson);
        
        cJSON_AddNumberToObject(ObjInfo, "PersonNum", 1);
        cJSON_AddItemToObject(ObjInfo, "PersonInfoList", pPersonInfoListJson);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_VEHICLE_BICYCLE || 
             p_alarm_event->alarm_level == ALARM_AI_VEHICLE_MOTO)
    {
        pNonMotorVehicleInfoJson = NULL;
        pNonMotorVehicleInfoListJson = cJSON_CreateArray();
        alarm_objinfo_non_motor_vehicle_get(p_alarm_event, &pNonMotorVehicleInfoJson);
        cJSON_AddItemToArray(pNonMotorVehicleInfoListJson, pNonMotorVehicleInfoJson);
        
        cJSON_AddNumberToObject(ObjInfo, "NonMotorVehicleNum", 1);
        cJSON_AddItemToObject(ObjInfo, "NonMotorVehicleInfoList", pNonMotorVehicleInfoListJson);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_VEHICLE_CAR || 
             p_alarm_event->alarm_level == ALARM_AI_VEHICLE_ELECTRICBICYCLE)
    {
        pVehicleInfoJson = NULL;
        pVehicleInfoListJson = cJSON_CreateArray();
        alarm_objinfo_vehicle_get(p_alarm_event, &pVehicleInfoJson);
        cJSON_AddItemToArray(pVehicleInfoListJson, pVehicleInfoJson);
        
        cJSON_AddNumberToObject(ObjInfo, "VehicleNum", 1);
        cJSON_AddItemToObject(ObjInfo, "VehicleInfoList", pVehicleInfoListJson);
    }
    else
    {
        cJSON_AddNumberToObject(ObjInfo, "PersonNum", 0);
        cJSON_AddNumberToObject(ObjInfo, "NonMotorVehicleNum", 0);
        cJSON_AddNumberToObject(ObjInfo, "VehicleNum", 0);
    }
    
    *pJson = ObjInfo;
    return 0;
}

int image_photograph_info_json_get(cJSON **p)
{
    char cmd_str[256] = {0};

    cJSON *ImageInfoJson = cJSON_CreateObject();
    if (ImageInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject ImageInfoJson failed\n");
        return -1;
    }

    cJSON_AddNumberToObject(ImageInfoJson, "Type", 23);
    cJSON_AddNumberToObject(ImageInfoJson, "Format", 0);

    struct tm stTime = {0};
    SystemLocalTime(&stTime);

    char jpegFile[128] = {0};
    snprintf(jpegFile, sizeof(jpegFile), "%04d%02d%02d%02d%02d%02d.jpg",
            stTime.tm_year + 1900,
            stTime.tm_mon + 1,
            stTime.tm_mday, 
            stTime.tm_hour,
            stTime.tm_min,
            stTime.tm_sec);

    unsigned long long filelen = 0;
    int picturesize = 0;
    int Stream = 1;
    int Quality = 15;
    unsigned char *JPGBuf = NULL;
    const char *filepath = "/tmp";

    anj_snap_jpg(0, Stream, Quality, (char *)filepath, jpegFile, NULL);

    char filePath[160] = {0};
    snprintf(filePath, sizeof(filePath), "%s/%s", filepath, jpegFile);
    if (0 == anj_snap_wait_complete(filePath, 5000))
    {
        anj_mw_read_file_len(filePath, &filelen);

        picturesize = (int)filelen;
        JPGBuf = (unsigned char *)anj_mw_malloc(picturesize);
        anj_mw_read_file_limit_len(filePath, (char *)JPGBuf, picturesize);
    }

    if (picturesize == 0 || JPGBuf == NULL)
    {
        __ERR("picturesize == 0 || JPGBuf == NULL\n");
        cJSON_Delete(ImageInfoJson);
        ImageInfoJson = NULL;
    }
    else
    {
        cJSON_AddNumberToObject(ImageInfoJson, "Width", DEFAULT_SMART_WIDTH);
        cJSON_AddNumberToObject(ImageInfoJson, "Height", DEFAULT_SMART_HEIGHT);
        cJSON_AddNumberToObject(ImageInfoJson, "CaptureTime", time(NULL));
        cJSON_AddNumberToObject(ImageInfoJson, "DataType", 0);
        
        int len = picturesize;
        int base64_len = len / 3 * 4 + (len % 3 != 0) * 4 + 1;
        
        char *base64 = (char *)anj_mw_malloc(base64_len);
        if (base64 == NULL)
        {
            __ERR("malloc base64 failed\n");
            cJSON_Delete(ImageInfoJson);
            anj_mw_free(JPGBuf);
            
            snprintf(cmd_str, sizeof(cmd_str), "rm %s", filePath);
            anj_mw_system(cmd_str);
            return -1;
        }

        memset(base64, '\0', sizeof(char) * base64_len);
        Base64Encode(JPGBuf, len, base64);

        cJSON_AddNumberToObject(ImageInfoJson, "Size", base64_len);
        cJSON_AddStringToObject(ImageInfoJson, "Data", base64);
        cJSON_AddStringToObject(ImageInfoJson, "URL", "");
        
        anj_mw_free(base64);
        base64 = NULL;

        *p = ImageInfoJson;
    }
    
    if (JPGBuf != NULL)
    {
        anj_mw_free(JPGBuf);
        JPGBuf = NULL;
    }

    snprintf(cmd_str, sizeof(cmd_str), "rm %s", filePath);
    anj_mw_system(cmd_str);

    return 0;
}

int image_info_json_get(char *snap_path, cJSON **p)
{
    cJSON *ImageInfoJson = cJSON_CreateObject();
    if (ImageInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject ImageInfoJson failed\n");
        return -1;
    }

    cJSON_AddNumberToObject(ImageInfoJson, "Type", 23);
    cJSON_AddNumberToObject(ImageInfoJson, "Format", 0);

    unsigned long long filelen = 0;
    anj_mw_read_file_len(snap_path, &filelen);

    unsigned char *JPGBuf = NULL;
    int nFileSize = (int)filelen;

    if (nFileSize > 0)
    {
        JPGBuf = (unsigned char *)anj_mw_malloc(nFileSize);
        anj_mw_read_file_limit_len(snap_path, (char *)JPGBuf, nFileSize);
    }

    if (nFileSize == 0 || JPGBuf == NULL)
    {
        __ERR("file:%s read len:%d error\n", snap_path, nFileSize);
        if (ImageInfoJson != NULL)
        {
            cJSON_Delete(ImageInfoJson);
            ImageInfoJson = NULL;
        }
        
        *p = NULL;
    }
    else
    {
        time_t current_time = time(NULL);
        cJSON_AddNumberToObject(ImageInfoJson, "Width", DEFAULT_SMART_WIDTH);
        cJSON_AddNumberToObject(ImageInfoJson, "Height", DEFAULT_SMART_HEIGHT);
        cJSON_AddNumberToObject(ImageInfoJson, "CaptureTime", current_time);
        cJSON_AddNumberToObject(ImageInfoJson, "DataType", 0);

        int len = nFileSize;
        int base64_len = len / 3 * 4 + (len % 3 != 0) * 4 + 1;

        char *base64 = (char *)anj_mw_malloc(base64_len);
        if (base64 == NULL)
        {
            __ERR("malloc base64 failed\n");
            cJSON_Delete(ImageInfoJson);
            anj_mw_free(JPGBuf);
            return -1;
        }
        
        memset(base64, '\0', sizeof(char) * base64_len);

        Base64Encode(JPGBuf, len, base64);
        cJSON_AddNumberToObject(ImageInfoJson, "Size", base64_len);
        cJSON_AddStringToObject(ImageInfoJson, "Data", base64);
        cJSON_AddStringToObject(ImageInfoJson, "URL", "");

        if (base64 != NULL)
        {
            anj_mw_free(base64);
            base64 = NULL;
        }

        *p = ImageInfoJson;
    }

    if (JPGBuf != NULL)
    {
        anj_mw_free(JPGBuf);
        JPGBuf = NULL;
    }

    return 0;
}


//获取报警信息
int alarm_event_info_get(alarm_event_data *p_alarm_event, int flag, int type, char *p)
{
    /*1.初始化AlarmInfo*/ 
    /*int time = (my_alarm_data->alarmtime.year-1970)*12*30*24*60*60 +
    (my_alarm_data->alarmtime.month)*30*24*60*60    +
    (my_alarm_data->alarmtime.day)*24*60*60 +
    (my_alarm_data->alarmtime.hour)*60*60    +
    (my_alarm_data->alarmtime.minute)*60    +
    (my_alarm_data->alarmtime.second);*/
    //人1，汽车2，摩托车3，客车货车4，自行车5

    long now_time = time(NULL);

    cJSON *AlarmInfoJson = cJSON_CreateObject(); // free 1
    if (AlarmInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject AlarmInfoJson failed\n");
        return -1;
    }

    if (type == 0)
    {
        if (flag == 1)
        {
            cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "FieldDetectorObjectsInside");
        }
        else
        {
            cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "FieldDetectorObjectsInside");
        }
    }
    else if(type == 1)
    {
        if (p_alarm_event->alarm_code == 52 || p_alarm_event->alarm_code == 53)
        {
            if (p_alarm_event->alarm_code == 52)
                cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "SmartMotionDetectOn");
            else if(p_alarm_event->alarm_code == 53)
                cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "SmartMotionDetectOff");
        }
        else if (p_alarm_event->alarm_level == 1)
        {
            cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "HumanShapeDetect");
        }
        else if (p_alarm_event->alarm_level == 2 || p_alarm_event->alarm_level == 4)
        {
            cJSON_AddStringToObject(AlarmInfoJson, "AlarmType", "VehicleDetect");
        }
    }

    //cJSON_AddStringToObject(AlarmInfo, "AlarmType", "VehicleDetect");
    cJSON_AddStringToObject(AlarmInfoJson, "AlarmLevel", "0");
    cJSON_AddNumberToObject(AlarmInfoJson, "TimeStamp", now_time);

    char *AlarmInfo_pri = NULL;
    AlarmInfo_pri = cJSON_Print(AlarmInfoJson);
    memcpy(p, AlarmInfo_pri, strlen(AlarmInfo_pri)); // free 2

    if ( NULL != AlarmInfoJson )
    {
        cJSON_Delete(AlarmInfoJson);
        AlarmInfoJson = NULL;
    }
    if ( NULL != AlarmInfo_pri )
    {
        free(AlarmInfo_pri);
        AlarmInfo_pri = NULL;
    }

    return 0;
}

int alarm_event_relation_object_info_get(alarm_event_data *p_alarm_event, char *p)
{
    cJSON *RelatedObjectInfo     = cJSON_CreateObject(); // free 1
    cJSON *ObjectList            = cJSON_CreateArray(); // free 2
    cJSON *ObjectIDInfo            = cJSON_CreateObject(); // free 3

    cJSON_AddNumberToObject(RelatedObjectInfo, "ObjectNum", 1);

    if (p_alarm_event->alarm_level == ALARM_AI_FACEDETECT)
        cJSON_AddNumberToObject(ObjectIDInfo, "ObjectType", 1);    
    else if (p_alarm_event->alarm_level == ALARM_AI_PD)    
        cJSON_AddNumberToObject(ObjectIDInfo, "ObjectType", 2);    
    else if (p_alarm_event->alarm_level == ALARM_AI_VEHICLE_CAR || 
            p_alarm_event->alarm_level == ALARM_AI_VEHICLE_MOTO || 
            p_alarm_event->alarm_level == ALARM_AI_VEHICLE_ELECTRICBICYCLE || 
            p_alarm_event->alarm_level == ALARM_AI_VEHICLE_BICYCLE)
    {
        cJSON_AddNumberToObject(ObjectIDInfo, "ObjectType", 3);
    }
    else if (p_alarm_event->alarm_level == ALARM_AI_FACEDETECT)     // 上面有重复的??
        cJSON_AddNumberToObject(ObjectIDInfo, "ObjectType", 4);
    else
        cJSON_AddNumberToObject(ObjectIDInfo, "ObjectType", 255);

    cJSON_AddNumberToObject(ObjectIDInfo, "ObjectID", 1);
    cJSON_AddItemToArray(ObjectList, ObjectIDInfo);
    cJSON_AddItemToObject(RelatedObjectInfo, "ObjectList", ObjectList);

    char *RelatedObjectInfo_pri = cJSON_Print(RelatedObjectInfo);
    memcpy(p, RelatedObjectInfo_pri, strlen(RelatedObjectInfo_pri)); // free 4

    if ( NULL != RelatedObjectInfo )
    {
        cJSON_Delete(RelatedObjectInfo);
        RelatedObjectInfo = NULL;
    }

    if ( NULL != RelatedObjectInfo_pri )
    {
        free(RelatedObjectInfo_pri);
        RelatedObjectInfo_pri = NULL;
    }

    return 0;
}

int alarm_event_structure_info_get(alarm_event_data *p_alarm_event, char *snap_path, cJSON **pMyJson)
{
    int ImageNum = 0;
    cJSON *StructureInfoJson = cJSON_CreateObject(); 
    if (StructureInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject StructureInfoJson failed\n");
        return -1;
    }

    cJSON *ObjInfo = NULL;
    int iRet = alarm_objinfo_get(p_alarm_event, &ObjInfo);
    if (ObjInfo == NULL || iRet == -1)
    {
        if ( NULL != StructureInfoJson)
        {
            cJSON_Delete(StructureInfoJson);
            StructureInfoJson = NULL;
        }
        return -1;
    }

    cJSON_AddItemToObject(StructureInfoJson, "ObjInfo", ObjInfo);
    cJSON *ImageInfo1 = NULL;
    cJSON *ImageInfo2 = NULL;
    cJSON *ImageInfoList = cJSON_CreateArray();

    if (0)
    {
        ;
    }
    else if (snap_path != NULL && (strlen(snap_path) > 0))  // 带图片路径，直接解析路径
    {
        cJSON *root    = cJSON_Parse(snap_path);
        if (NULL != root)
        {
            cJSON *path    = cJSON_GetObjectItem(root, "path");
            if (path != NULL)
            {
                if (anj_mw_file_exists(path->valuestring))
                {
                    image_info_json_get(path->valuestring, &ImageInfo1);
                }
            }

            cJSON_Delete(root);
            root = NULL;
        }
    }
    else                                                    // 不带图片路径，主动抓一张
    {
        anj_mw_system("rm /tmp/*.jpg");
        image_photograph_info_json_get(&ImageInfo1);
    }

    if (ImageInfo1 != NULL)
    {
        ImageNum ++;
        ImageInfo2 = cJSON_Duplicate(ImageInfo1, 1);
        cJSON_AddNumberToObject(ImageInfo1, "Index", 1);
        cJSON_AddItemToArray(ImageInfoList, ImageInfo1);
    }
    if (ImageInfo2 != NULL)
    {
        ImageNum ++;
        cJSON_AddNumberToObject(ImageInfo2, "Index", 2);
        cJSON_AddItemToArray(ImageInfoList, ImageInfo2);
    }

    if (ImageNum == 2)
    {
        cJSON_AddNumberToObject(StructureInfoJson, "ImageNum", ImageNum);
        cJSON_AddItemToObject(StructureInfoJson, "ImageInfoList", ImageInfoList);
    }
    else
    {
        cJSON_AddNumberToObject(StructureInfoJson, "ImageNum", 0);
        if ( NULL != ImageInfoList )
        {
            cJSON_Delete(ImageInfoList);
            ImageInfoList = NULL;
        }
    }

    *pMyJson = StructureInfoJson;
    return 0;
}


//删掉相同IP/PORT的订阅，避免重复订阅
static void unv_subsmap_delete_same_ip_port(char *ip_buf, int port)
{
    set<unsigned int> setToErase;

    anj_mutex_lock(&s_stUnvSubMutex);
    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.begin();
    for(; it != s_UnvSubMap.end(); ++it)
    {
        unsigned int ID = it->first;
        Unv_Sub_Node &data = it->second;

        if(strcmp(data.ipaddr_port, ip_buf) == 0 && data.port == port)
        {
            setToErase.insert(ID);
            __INFO("unv subsmap: %s:%d, ID:%d repeated, delete it.\n", data.ipaddr_port, data.port, data.ID);
        }
    }

    set<unsigned int>::iterator it2 = setToErase.begin();
    for(; it2 != setToErase.end(); ++it2)
    {
        unsigned int ID = *it2;
        s_UnvSubMap.erase(ID);
    }

    anj_mutex_unlock(&s_stUnvSubMutex);  
}

//检查订阅表的订阅时间
static void unv_subsmap_check_timeout()
{
    unsigned long long uNow = GetCurrentTimeStampU64();
    set<unsigned int> setToErase;

    anj_mutex_lock(&s_stUnvSubMutex); 
    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.begin();
    for(; it != s_UnvSubMap.end(); ++it)
    {
        unsigned int ID = it->first;
        Unv_Sub_Node &data = it->second;

        if(data.time_end < uNow)
        {
            setToErase.insert(ID);
            __INFO("unv subsmap: %s:%d, ID %d check timeout, delete it.\n", data.ipaddr_port, data.port, data.ID);
        }
    }
    
    set<unsigned int>::iterator it2 = setToErase.begin();
    for(; it2 != setToErase.end(); ++it2)
    {
        unsigned int ID = *it2;
        s_UnvSubMap.erase(ID);
    }

    anj_mutex_unlock(&s_stUnvSubMutex);     
}

unsigned int unv_subscribe_map_size_get()
{
    unsigned int nSize = s_UnvSubMap.size();
    return nSize;
}

// 添加订阅节点
int unv_subscribe_add(char *ip_buf, int port, long seconds, unsigned int id)
{
    //处理新的订阅时删掉过期订阅，避免开启线程定时检查
    unv_subsmap_check_timeout();

    __DBG("unv subs:%s:%d: subscribe add %d seconds, id:%u\n", ip_buf, port, seconds, id);
    unv_subsmap_delete_same_ip_port(ip_buf, port);

    Unv_Sub_Node stNode = {0};
    strcpy(stNode.ipaddr_port, ip_buf);
    stNode.port = port;
    stNode.time_end = GetCurrentTimeStampU64() + seconds * 1000;
    stNode.ID = id;

    anj_mutex_lock(&s_stUnvSubMutex); 
    s_UnvSubMap[id] = stNode;
    anj_mutex_unlock(&s_stUnvSubMutex);    

    return 0;
}

// 删除订阅节点
int unv_subscribe_delete(unsigned int id)
{
    //处理新的订阅时删掉过期订阅，避免开启线程定时检查
    anj_mutex_lock(&s_stUnvSubMutex);

    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.find(id);
    if (it != s_UnvSubMap.end())
    {
        Unv_Sub_Node &data = it->second;
        data.time_end = 0;
    }
    anj_mutex_unlock(&s_stUnvSubMutex);

    unv_subsmap_check_timeout();
    __INFO("unv subs: delete id:%u node, now subsmap size:%u\n", unv_subscribe_map_size_get());
    return 0;
}

//订阅刷新
int unv_subscribe_refresh(long seconds, unsigned int id, Unv_Sub_Node **pGetNode)
{
    int iRet = -1;
    anj_mutex_lock(&s_stUnvSubMutex);
    
    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.find(id);
    if(it != s_UnvSubMap.end())
    {
        Unv_Sub_Node &data = it->second;
        data.time_end = GetCurrentTimeStampU64() + seconds * 1000;
        __DBG("unv subs:%s:%d refresh:%d seconds, id:%u\n", data.ipaddr_port, data.port, seconds, id);

        iRet = 0;
        *pGetNode = &data;
    }
    else
    {
        __ERR("unv subs: refresh %d seconds for id:%u failed\n", seconds, id);
        *pGetNode = NULL;
    }
    anj_mutex_unlock(&s_stUnvSubMutex);

    //订阅刷新时删掉过期订阅，避免开启线程定时检查
    unv_subsmap_check_timeout();
    return iRet;
}

int unv_send_msg_to_node(char *msg, Unv_Sub_Node *pNode)
{
    char ip_buf[32] = {0};
    int port = 0;
    unsigned long long end_time = 0;
    unsigned int ID_num = 0;

    int sockfd = 0;
    int iRet = 0;
    struct sockaddr_in servaddr;
    socklen_t len = 0;

    strcpy(ip_buf, pNode->ipaddr_port);
    port = pNode->port;
    end_time = pNode->time_end;
    ID_num = pNode->ID;

    unsigned long long cur_time = GetCurrentTimeStampU64();
    if (cur_time > end_time)
    {
        return 2;
    }

    char ip_buf1[64] = {0};
    snprintf(ip_buf1, sizeof(ip_buf1), "Host: %s:%d\n", ip_buf, port);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        __ERR("--- socket error!\n");
        return -1;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip_buf, &servaddr.sin_addr) <= 0)
    {
        __ERR("--- inet_pton error!\n");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        __ERR("--- connect error!\n");
        close(sockfd);
        return -1;
    }

    cJSON *root = cJSON_Parse(msg);
    if (root == NULL)
    {
        __ERR("cJSON_Parse msg failed\n");
        close(sockfd);
        return -1;
    }

    cJSON *root1 = cJSON_CreateObject();
    cJSON *AlarmInfo = cJSON_GetObjectItem(root, "AlarmInfo");
    cJSON *RelatedObjects = cJSON_GetObjectItem(root, "RelatedObjects");

    char ip_addr[64] = {0};
    int web_port = 0;
    http_get_ip_addr_port(ip_addr, sizeof(ip_addr), &web_port);

    char str_Reference[128] = {0};
    char ref[64] = "%s:%d/Subscription/Subscribers/%d";
    snprintf(str_Reference, sizeof(str_Reference), ref, ip_addr, web_port, ID_num);
    cJSON_AddStringToObject(root1, "Reference", str_Reference);
    
    char *AlarmInfo_pri = cJSON_Print(AlarmInfo);
    cJSON *AlarmInfo_pri_toC = NULL;
    if (AlarmInfo_pri != NULL)
    {
        AlarmInfo_pri_toC = cJSON_Parse(AlarmInfo_pri);
    }
    
    char *RelatedObjects_pri = cJSON_Print(RelatedObjects);
    cJSON *RelatedObjects_pri_toC = NULL;
    if (RelatedObjects_pri != NULL)
    {
        RelatedObjects_pri_toC = cJSON_Parse(RelatedObjects_pri);
    }
    
    if (AlarmInfo_pri_toC != NULL)
    {
        cJSON_AddItemToObject(root1, "AlarmInfo", AlarmInfo_pri_toC);
    }
    
    if (RelatedObjects_pri_toC != NULL)
    {
        cJSON_AddItemToObject(root1, "RelatedObjects", RelatedObjects_pri_toC);
    }
    
    char *msg2 = cJSON_Print(root1);

    char str1[4096] = {0};
    char str2[4096] = {0};
    if (msg2 != NULL)
    {
        strcat(str2, msg2);
    }
    
    char *str = (char *)anj_mw_malloc(128);
    if (str == NULL)
    {
        __ERR("str malloc failed\n");
        goto __cleanup;
    }
    
    len = strlen(str2);
    sprintf(str, "%d", len);
    
    strcat(str1, "POST /LAPI/V1.0/System/Event/Notification/Alarm HTTP/1.1\n");
    strcat(str1, ip_buf1);
    strcat(str1, "Accept: */*\n");
    strcat(str1, "cache-control: no-cache\n");
    strcat(str1, "content-type: multipart/form-data\n");
    strcat(str1, "Content-Length: ");
    strcat(str1, str);
    strcat(str1, "\n\n");
    
    if (str2 != NULL)
    {
        strcat(str1, str2);
    }
    
    strcat(str1, "\r\n\r\n");

    iRet = write(sockfd, str1, strlen(str1));
    if (iRet < 0)
    {
        __ERR("send fd:%d err errno:%d %s\n", sockfd, errno, strerror(errno));
        iRet = -1;
    }
    else
    {
        iRet = 0;
    }

__cleanup:
    close(sockfd);
    
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
    
    if (root1 != NULL)
    {
        cJSON_Delete(root1);
    }
    
    if (AlarmInfo_pri != NULL)
    {
        anj_mw_free(AlarmInfo_pri);
    }
    
    if (RelatedObjects_pri != NULL)
    {
        anj_mw_free(RelatedObjects_pri);
    }
    
    if (msg2 != NULL)
    {
        anj_mw_free(msg2);
    }
    
    if (str != NULL)
    {
        anj_mw_free(str);
    }
    
    return iRet;
}

int unv_send_alarm_msg_to_node(char *msg, Unv_Sub_Node *pNode)
{
    if (msg == NULL || pNode == NULL)
    {
        return 0;
    }

    int iRet = 0;
    unsigned long long cur_time = GetCurrentTimeStampU64();
    if (cur_time > pNode->time_end)
    {
        __INFO("Now time:%llu > pNode->time_end:%llu\n", cur_time, pNode->time_end);
        return 2;
    }

#ifdef CURL_SUPPORT
    __DBG("send msg by curl!\n");

    curl_global_cleanup();
    CURL *pCurlHandle = curl_easy_init();

    char url[256] = {0};
    char str_Reference[256] = {0};

    snprintf(url, sizeof(url), "http://%s:%d/LAPI/V1.0/System/Event/Notification/Alarm", 
            pNode->ipaddr_port, pNode->port);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "cache-control: no-cache");
    headers = curl_slist_append(headers, "content-type: multipart/form-data");
    
    cJSON *root = cJSON_Parse(msg);
    cJSON *root1 = cJSON_CreateObject();
    cJSON *AlarmInfo = cJSON_GetObjectItem(root, "AlarmInfo");
    cJSON *RelatedObjects = cJSON_GetObjectItem(root, "RelatedObjects");
    
    snprintf(str_Reference, sizeof(str_Reference), "%s:%d/Subscription/Subscribers/%d", 
            g_Yen_IPAddr, g_Yen_Port, pNode->ID);
    
    cJSON_AddStringToObject(root1, "Reference", str_Reference);
    
    if (AlarmInfo != NULL && RelatedObjects != NULL)
    {
        cJSON_AddItemToObject(root1, "AlarmInfo", cJSON_Duplicate(AlarmInfo, 1)); 
        cJSON_AddItemToObject(root1, "RelatedObjects", cJSON_Duplicate(RelatedObjects, 1)); 
    }
    
    char *msg2 = cJSON_Print(root1);

    curl_easy_setopt(pCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(pCurlHandle, CURLOPT_URL, url);
    curl_easy_setopt(pCurlHandle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, msg2);
    curl_easy_setopt(pCurlHandle, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    curl_easy_setopt(pCurlHandle, CURLOPT_TIMEOUT_MS, 2000);
    
    CURLcode rett = curl_easy_perform(pCurlHandle);
    if (rett != CURLE_OK)
    {
        pNode->send_failed_time++;
        if (pNode->send_failed_time >= 3)
        {
            iRet = 2;
        }
    }
    else
    {
        iRet = 0;
    }
    
    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    
    if (root1 != NULL)
    {
        cJSON_Delete(root1);
        root1 = NULL;
    }

    anj_mw_free(msg2);
    curl_slist_free_all(headers);
    curl_easy_cleanup(pCurlHandle);
    curl_global_cleanup();
    
    return iRet;
#else
    __DBG("send msg by manual!\n");
    int sockfd = 0;
    char *send_buf = NULL;
    char buf[1024] = {0};
    int buf_len = 0;
    int send_buf_len = 0;
    int msg_len = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    iRet = os_socket_connect(sockfd, pNode->ipaddr_port, pNode->port, 2000);
    if (iRet != -1)
    {
        msg_len = strlen(msg);
        snprintf(buf, sizeof(buf), unv_str_alarm_head, pNode->ipaddr_port, pNode->port, msg_len);
        buf_len = strlen(buf);
        send_buf_len = msg_len + buf_len;
        
        send_buf = (char *)anj_mw_malloc(send_buf_len);
        if (send_buf == NULL)
        {
            __ERR("send_buf malloc failed\n");
            close(sockfd);
            return -1;
        }
        
        memset(send_buf, 0, send_buf_len);
        memcpy(send_buf, buf, buf_len);
        memcpy(send_buf + buf_len, msg, msg_len);
        
        iRet = os_socket_send(sockfd, send_buf, send_buf_len, 2000);
        if (iRet == send_buf_len)
        {
            ;
        }
        else if (iRet > 0)
        {
            __ERR("send buf but len error:%d!\n", iRet);
        }
        else
        {
            __ERR("send buf error! failed times:%d!\n", pNode->send_failed_time);
            pNode->send_failed_time++;
            if (pNode->send_failed_time >= 3)
            {
                iRet = 2;
            }
        }
    }
    else
    {
        __ERR("socket connect failed! failed times:%d!\n", pNode->send_failed_time);
        pNode->send_failed_time++;
        if (pNode->send_failed_time >= 3)
        {
            iRet = 2;
        }
    }

    close(sockfd);

    if (send_buf != NULL)
    {
        anj_mw_free(send_buf);
        send_buf = NULL;
    }
    
    return iRet;
#endif
}

int unv_send_structure_msg_to_node(const char *msg, Unv_Sub_Node *pNode, int msg_len)
{
    if (msg == NULL || pNode == NULL)
    {
        return 0;
    }

    int iRet = 0;
    unsigned long long cur_time = GetCurrentTimeStampU64();
    if (cur_time > pNode->time_end)
    {
        __INFO("Now time:%llu > pNode->time_end:%llu\n", cur_time, pNode->time_end);
        return 2;
    }

#ifdef CURL_SUPPORT
    __DBG("send msg by curl!\n");
    curl_global_cleanup();
    CURL *pCurlHandle = curl_easy_init();

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "cache-control: no-cache");
    headers = curl_slist_append(headers, "content-type: multipart/form-data");

    char url[256] = {0};
    snprintf(url, sizeof(url), "http://%s:%d/LAPI/V1.0/System/Event/Notification/Structure", 
            pNode->ipaddr_port, pNode->port);

    curl_easy_setopt(pCurlHandle, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(pCurlHandle, CURLOPT_URL, url);
    curl_easy_setopt(pCurlHandle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, msg); 
    curl_easy_setopt(pCurlHandle, CURLOPT_CONNECTTIMEOUT_MS, 2000);
    curl_easy_setopt(pCurlHandle, CURLOPT_TIMEOUT_MS, 2000);

    CURLcode rett = curl_easy_perform(pCurlHandle);
    if (rett != CURLE_OK)
    {
        pNode->send_failed_time++;
        if (pNode->send_failed_time >= 3)
        {
            iRet = 2;
        }
    }
    else
    {
        iRet = 0;
    }
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(pCurlHandle);
    curl_global_cleanup();
    
    return iRet;
#else
    __DBG("send msg by manual!\n");
    int sockfd = 0;

    int buf_len = 0;
    char buf[1024] = {0};
    int send_buf_len = 0;
    char *send_buf = NULL;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd <= 0)
    {
        __ERR("socket error!\n");
        return -1;
    }

    iRet = os_socket_connect(sockfd, pNode->ipaddr_port, pNode->port, 2000);
    if (iRet != -1)
    {
        snprintf(buf, sizeof(buf), unv_str_structure_head, pNode->ipaddr_port, pNode->port, msg_len);
        buf_len = strlen(buf);
        send_buf_len = msg_len + buf_len + 1;
        
        send_buf = (char *)anj_mw_malloc(send_buf_len);
        if (send_buf == NULL)
        {
            __ERR("send_buf malloc failed\n");
            close(sockfd);
            return -1;
        }
        
        memset(send_buf, 0, send_buf_len);

        memcpy(send_buf, buf, buf_len);
        memcpy(send_buf + buf_len, msg, msg_len);

        iRet = os_socket_send(sockfd, send_buf, send_buf_len, 2000);
        if (iRet == send_buf_len)
        {
            ;
        }
        else if (iRet > 0)
        {
            __ERR("send buf but len error:%d!!!\n", iRet);
        }
        else
        {
            __ERR("send buf error! failed times:%d\n", pNode->send_failed_time);
            pNode->send_failed_time++;
            if (pNode->send_failed_time >= 3)
            {
                iRet = 2;
            }
        }
    }
    else
    {
        __ERR("socket connect failed, failed times:%d!!\n", pNode->send_failed_time);
        pNode->send_failed_time++;
        if (pNode->send_failed_time >= 3)
        {
            iRet = 2;
        }
    }

    close(sockfd);

    if (send_buf != NULL)
    {
        anj_mw_free(send_buf);
        send_buf = NULL;
    }

    return iRet;
#endif
}


//告警触发，向list节点全部发送告警
int unv_subscribe_send_msg_to_listnode(const char* msg)
{
    if( NULL == msg)
        return 0;

    int iRet = 0;
    int msg_len = 0;
    char buf[1024] = {0};

    char ip_addr[64] = {0};
    int port = 0;
    http_get_ip_addr_port(ip_addr, sizeof(ip_addr), &port);

    string msg_tmp = msg;
    map<unsigned int, void*> setToErase;

    anj_mutex_lock(&s_stUnvSubMutex);

    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.begin();
    for(; it != s_UnvSubMap.end(); ++it)
    {
        unsigned int ID = it->first;
        Unv_Sub_Node &data = it->second;
        
        Unv_Sub_Node *Node = &data;
        __DBG("unv subs: node info ipaddr:%s, port:%d, time_end:%llu, id:%u\n",
            Node->ipaddr_port, Node->port, Node->time_end, Node->ID);

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), unv_str_subscribe_head, ip_addr, port, Node->ID);

        msg_tmp = msg;
        msg_tmp.insert(2, buf);
        msg_len = msg_tmp.size();

        iRet = unv_send_structure_msg_to_node((char*)msg_tmp.data(), &data, msg_len);
        if (iRet == 2)   //等于2则去掉该节点
        {
            setToErase[ID] = NULL;      //不push该节点
            __ERR("iRet = 2, don't erase this id:%u\n", ID);
        }
    }

    map<unsigned int, void*>::iterator it2 = setToErase.begin();
    for(; it2 != setToErase.end(); ++it2)
    {
        unsigned int ID = it2->first;
        s_UnvSubMap.erase(ID);
    }

    anj_mutex_unlock(&s_stUnvSubMutex);
    return 0;
}


//告警触发，向list节点全部发送告警
int unv_subscribe_send_alarminfo_to_listnode(const char* AlarmInfo, const char* RelatedObjectInfo)
{
    if( NULL == AlarmInfo || NULL == RelatedObjectInfo)
    {
        return 0;
    }

    int iRet = 0;
    map<unsigned int, void*> setToErase;

    anj_mutex_lock(&s_stUnvSubMutex);
    map<unsigned int, Unv_Sub_Node>::iterator it = s_UnvSubMap.begin();
    for(; it != s_UnvSubMap.end(); ++it)
    {
        unsigned int ID = it->first;
        Unv_Sub_Node &data = it->second;
        
        Unv_Sub_Node *Node = &data;
        __DBG("unv subs: node info ipaddr:%s, port:%d, time_end:%llu, id:%u\n",
            Node->ipaddr_port, Node->port, Node->time_end, Node->ID);

        char Reference[128] = {0};
        const char *ref= "%s:%d/Subscription/Subscribers/%d";
        snprintf(Reference, sizeof(Reference), ref, Node->ipaddr_port, Node->port, Node->ID);

        cJSON *root = cJSON_CreateObject(); //free 5
        cJSON *AlarmInfox = cJSON_Parse(AlarmInfo); //free 6
        cJSON *RelatedObjects = cJSON_Parse(RelatedObjectInfo); //free 7
        cJSON_AddStringToObject(root, "Reference", Reference);
        cJSON_AddItemToObject(root, "AlarmInfo", AlarmInfox);
        cJSON_AddItemToObject(root, "RelatedObjects", RelatedObjects);

        char * msg = cJSON_Print(root); //free 8
        cJSON_Delete(root);
        if(NULL != msg)
        {
            iRet = unv_send_alarm_msg_to_node(msg, Node);
            if (iRet == 2)  //等于2则去掉该节点
            {
                setToErase[ID] = NULL;
                __ERR("iRet = 2, don't erase this id:%u\n", ID);
            }

            anj_mw_free(msg);
            msg = NULL;
        }
    }

    map<unsigned int, void*>::iterator it2 = setToErase.begin();
    for(; it2 != setToErase.end(); ++it2)
    {
        unsigned int ID = it2->first;
        s_UnvSubMap.erase(ID);
    }

    anj_mutex_unlock(&s_stUnvSubMutex);

    return 0;
}


/*
    订阅报警发送流程：
    unv_notify_alarm_event_snap_to_sublist
    -> alarm_event_structure_info_get
        -> 组装报警规则信息 + 图片信息
    -> unv_subscribe_send_msg_to_listnode
        -> unv_send_structure_msg_to_node
        -> 发送信息到节点
*/
int unv_notify_alarm_event_snap_to_sublist(alarm_event_data *p_alarm_event, int type, char *snap_path)
{

    __DBG("notify alarm event with snap!\n");

    int count = unv_subscribe_map_size_get();
    if (count == 0)
    {
        return 0;
    }

#if 0
    sprintf(g_Yen_IPAddr, "%s", pNetworkCfg.lanCfg.IPAddress);
    FILE *web_port_fp = fopen("/tmp/web_port","wb");
    if(web_port_fp)
    {
        fwrite(&port, 1, sizeof(short), web_port_fp);
        fclose(web_port_fp);
    }
    printf( "Port (%d) bind successful: master socket = %d\n", port, master); 
    g_Yen_Port = port;
#endif

#if 0
    NETWORK_STATUS_DATA stNetworkStatus = {0};
    anj_net_info_get(&stNetworkStatus);
    char ip_addr[64] = {0};
    StrCpy(ip_addr, sizeof(ip_addr), stNetworkStatus.ip);

    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaConfig();
    int port = pstMediaStreamCfg->webConfig.webPort;

        char ref[128] = 
    "{\r\n\
        \"Reference\":  \"%s:%d/Subscription/Subscribers/%d\",\r\n";

    char Reference[128] = {0};
    snprintf(Reference, sizeof(Reference), ref, ip_addr, port, 0);
#endif

    time_t TimeStamp = time(NULL);
    int SrcID = 1;
    int NotificationType = 0;

// 1.组装alarm structure info json
    cJSON *pStructureInfoJson = NULL;
    int iRet = alarm_event_structure_info_get(p_alarm_event, snap_path, &pStructureInfoJson);
    if (pStructureInfoJson == NULL || iRet == -1)
    {
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "TimeStamp", TimeStamp);
    cJSON_AddNumberToObject(root, "SrcID", SrcID);
    cJSON_AddStringToObject(root, "SrcName", "Channe1");
    cJSON_AddNumberToObject(root, "NotificationType", NotificationType);
    
    cJSON_AddItemToObject(root, "StructureInfo", pStructureInfoJson);
    
    char *msg = cJSON_Print(root);

    if (root != NULL)
    {
        cJSON_Delete(root);
        root = NULL;
    }

// 2. 发送structure info 到所有订阅的节点
    if (msg != NULL)
    {
        unv_subscribe_send_msg_to_listnode(msg);

        anj_mw_free(msg);
        msg = NULL;
    }

    return 0;
}

/*
    订阅报警发送流程：
    unv_notify_alarm_event_to_sublist
    -> alarm_event_info_get
        -> 组装报警规则信息

    -> alarm_event_relation_object_info_get
        -> 组装relation信息

    -> unv_subscribe_send_alarminfo_to_listnode
        -> unv_send_alarm_msg_to_node
        -> 发送信息到节点
*/
int unv_notify_alarm_event_to_sublist(alarm_event_data *p_alarm_event, int type)
{
    int count = unv_subscribe_map_size_get();
    if (count == 0)
    {
        return 0;
    }

    unsigned int alarm_info_len = 1024;
    char *pAlarmInfo = (char *)anj_mw_malloc(alarm_info_len);
    if (pAlarmInfo == NULL)
    {
        __ERR("pAlarmInfo malloc failed\n");
        return -1;
    }
    
    memset(pAlarmInfo, 0, alarm_info_len);
    
    char *pRelatedObjectInfo = (char *)anj_mw_malloc(alarm_info_len);
    if (pRelatedObjectInfo == NULL)
    {
        __ERR("pRelatedObjectInfo RelatedObjectInfo failed\n");

        anj_mw_free(pAlarmInfo);
        pAlarmInfo = NULL;
        return -1;
    }
    
    memset(pRelatedObjectInfo, 0, alarm_info_len);

// 1. 组装alarminfo json
    if (type == 0)
    {
        if (p_alarm_event->alarm_code == 50 || p_alarm_event->alarm_code == 52)
        {
            alarm_event_info_get(p_alarm_event, 1, type, pAlarmInfo);
        }
        else if (p_alarm_event->alarm_code == 51 || p_alarm_event->alarm_code == 53)
        {
            alarm_event_info_get(p_alarm_event, 0, type, pAlarmInfo);  
        }
    }
    else if (type == 1)
    {
        if (p_alarm_event->alarm_code == 50 || p_alarm_event->alarm_code == 52 || 
            p_alarm_event->alarm_code == 54 || p_alarm_event->alarm_code == 56 || 
            p_alarm_event->alarm_code == 62 || p_alarm_event->alarm_code == 64)
        {
            alarm_event_info_get(p_alarm_event, 1, type, pAlarmInfo);
        }
        else if (p_alarm_event->alarm_code == 63 || p_alarm_event->alarm_code == 65)
        {
            alarm_event_info_get(p_alarm_event, 0, type, pAlarmInfo);  
        }
    }

// 2. 组装alarm relation json
    alarm_event_relation_object_info_get(p_alarm_event, pRelatedObjectInfo);

// 3.发送alarm info 到所有订阅的节点
    unv_subscribe_send_alarminfo_to_listnode(pAlarmInfo, pRelatedObjectInfo);

    if (pAlarmInfo != NULL)
    {
        anj_mw_free(pAlarmInfo);
        pAlarmInfo = NULL;
    }
    if (pRelatedObjectInfo != NULL)
    {
        anj_mw_free(pRelatedObjectInfo);
        pRelatedObjectInfo = NULL;
    }

    return 0;
}

