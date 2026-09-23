#ifndef __OTA_UPDATE_H_
#define __OTA_UPDATE_H_

int ota_check_version(int bDownload);

int ota_version_get(int *status, char *latestversion, char *releasenotes);

#endif