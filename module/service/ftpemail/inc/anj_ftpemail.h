#ifndef _ANJ_FTPEMAIL_H_
#define _ANJ_FTPEMAIL_H_

#include "anj_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define ANJ_FTPEMAIL_INFO_MAX_LEN 1024

int anj_ftpemail_ftp_file(FtpServer *param, const char *path, const char *filename,
                         const char *remotepath, const char *remotefile);
int anj_ftpemail_email_file(SmtpServerList *param, int account_index,
                            const char *path, const char *filename, const char *infomation);
int anj_ftpemail_ftp_test(FtpServer *param, const char *device_ip);
int anj_ftpemail_smtp_test(SmtpServerList *param, int account_index, const char *device_ip);

#ifdef __cplusplus
}
#endif

#endif
