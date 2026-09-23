#ifndef __ANJ_SERVICE_UPGRADE_H__
#define __ANJ_SERVICE_UPGRADE_H__

#include "ixml.h"

void anj_service_upgrade_prepare_memory(int MsgSrc);

int anj_service_upgrade_handle_upload_file(IXML_Document *pDoc, int MsgSrc, int lognum, char **msg_body);

int anj_service_sysctl_basic_get_serialnumber(char **msg_body);
int anj_service_sysctl_basic_get_systemcontrolstring(char **msg_body);
int anj_service_sysctl_basic_get_version_info(char **msg_body);
int anj_service_sysctl_basic_get_network_status(char **msg_body);
int anj_service_sysctl_basic_get_media_capability(char **msg_body);
int anj_service_sysctl_basic_set_system_time(IXML_Document *pDoc);

#endif
