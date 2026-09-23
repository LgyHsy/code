#ifndef __ANJ_PRI_CMD_H__
#define __ANJ_PRI_CMD_H__

#ifdef __cplusplus
extern "C"
{
#endif

int anj_pri_cmd_port_get(int udp_or_tcp);
int anj_pri_cmd_proc(char *cmdbuf, int cmdlen, int lognum, unsigned long ulLeadCode);

void anj_pri_cmd_wifi_list_reponse(void *ctx, int result, int lognum);
void anj_pri_cmd_formart_reponse(int result, int lognum);
void anj_pri_cmd_jpg_reponse(int result, int lognum, char *JpgFile);
void anj_pri_alarm_event_notify(void *alarm_data);

#ifdef __cplusplus
}
#endif

#endif
