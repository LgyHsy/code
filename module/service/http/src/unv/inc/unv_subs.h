#ifndef __UNV_SUBS_H__
#define __UNV_SUBS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct 
{
    char ipaddr_port[128];
    int port;
    unsigned long long time_end;
    unsigned int ID;
    unsigned int send_failed_time;
}Unv_Sub_Node;

unsigned int unv_subscribe_map_size_get();

// 添加订阅节点
int unv_subscribe_add(char *ip_buf, int port, long seconds, unsigned int id);

// 删除订阅节点
int unv_subscribe_delete(unsigned int id);

//订阅刷新
int unv_subscribe_refresh(long seconds, unsigned int id, Unv_Sub_Node **pGetNode);

int unv_notify_alarm_event_snap_to_sublist(alarm_event_data *p_alarm_event, int type, char *snap_path);

int unv_notify_alarm_event_to_sublist(alarm_event_data *p_alarm_event, int type);

#ifdef __cplusplus
}
#endif

#endif
