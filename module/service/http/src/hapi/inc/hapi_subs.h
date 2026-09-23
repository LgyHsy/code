#ifndef _HAPI_SUBS_H__
#define _HAPI_SUBS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    SERVER_TYPE_IPV4 = 0,
    SERVER_TYPE_DOMAINNAME = 1,
}ServerType;

typedef struct 
{
    ServerType nServerType;
    char szServerName[64];
    unsigned short nServerPort;
    char szUserName[64];
    char szPassword[64];
    char szPostURLPrefix[64];               // POST到server的URL前缀，用于将事件通知到某些报警服务器需要携带特定的前缀
    unsigned long long uptime_timeout;      //在某个系统运行时间超时
    time_t localtime_timeout;               //超时的系统本地时间
    char szEventType[512];                  //订阅的事件类型，可以分开订阅移动侦测、人形、车型、车牌等等
    unsigned int errortimes;
}HapiSubNodeStruct;

unsigned int hapi_submap_add(HapiSubNodeStruct *pAddNode);

int hapi_submap_refresh(unsigned int uid, unsigned int nDuration, HapiSubNodeStruct **pSubNode);

int hapi_submap_delete(unsigned int uid);

void hapi_alarm_event_notify(const alarm_event_data *pAlarm);

void hapi_add_alarmserver();
void hapi_destory_alarmserver();
    
int hapi_notify_uninit();

#ifdef __cplusplus
}
#endif

#endif

