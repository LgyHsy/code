#ifndef __ANJ_SERVICE_PROVIDER_H__
#define __ANJ_SERVICE_PROVIDER_H__

#include "anj_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    ANJ_SERVICE_PROVIDER_RTSP = 0,
    ANJ_SERVICE_PROVIDER_GB28181,
    ANJ_SERVICE_PROVIDER_WEB,
    ANJ_SERVICE_PROVIDER_H5LIVE,
    ANJ_SERVICE_PROVIDER_RTMP,
    ANJ_SERVICE_PROVIDER_ONVIF,
    ANJ_SERVICE_PROVIDER_FTP_EMAIL,
    ANJ_SERVICE_PROVIDER_HIK,
} anj_service_provider_type_e;

typedef enum
{
    ANJ_SERVICE_PROVIDER_CAP_NONE = 0,
    ANJ_SERVICE_PROVIDER_CAP_GB28181 = 1 << 0,
} anj_service_provider_cap_e;

typedef struct
{
    const char *provider_name;
    int provider_type;
    int provider_priority;
    unsigned int capability_flags;
    int (*init)(void);
    int (*uninit)(void);
    int (*alarm_event_notify)(void *alarm_event);
    int (*audio_enc_change)(void);
    int (*ftp_file)(FtpServer *param, const char *path, const char *filename,
                    const char *remotepath, const char *remotefile);
    int (*email_file)(SmtpServerList *param, int account_index,
                      const char *path, const char *filename, const char *infomation);
    int (*ftp_test)(FtpServer *param, const char *device_ip);
    int (*smtp_test)(SmtpServerList *param, int account_index, const char *device_ip);
} anj_service_provider_ops;

int anj_service_provider_register(const anj_service_provider_ops *ops);
void anj_service_provider_unregister(const anj_service_provider_ops *ops);
int anj_service_provider_init_all(void);
int anj_service_provider_uninit_all(void);
int anj_service_provider_uninit_single(int provider_type);
int anj_service_provider_alarm_event_notify(void *alarm_event);
int anj_service_provider_audio_enc_change(void);
int anj_service_provider_has_capability(unsigned int capability_flags);

#ifdef __cplusplus
}
#endif

#endif
