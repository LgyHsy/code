#ifndef __SN_TOOLS_H__
#define __SN_TOOLS_H__

int sn_net_get_flag(const char *ifname);

int sn_ifname_connected(const char *ifname);

void sn_get_ifname(char *ifname);

// 检查广播路由是否已经存在
// 返回 1 表示存在，0 表示不存在，-1 表示读取失败
int sn_broadcast_if_route_exist(const char *interface_name);

int sn_broadcast_if_route_add(const char *interface_name);

int sn_broadcast_route_add();

unsigned long long sn_get_runtime(void);

void sn_password(const char* sn, const char* uuid, char* out_password);

#endif /* __SN_TOOLS_H__ */