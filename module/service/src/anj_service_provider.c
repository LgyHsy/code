#include <stdlib.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_service_provider.h"
#include "anj_ftpemail.h"

typedef struct anj_service_provider_entry_s
{
    const anj_service_provider_ops *ops;
    int initialized;
    struct anj_service_provider_entry_s *next;
} anj_service_provider_entry_t;

static anj_service_provider_entry_t *s_stProviderList = NULL;

static anj_service_provider_entry_t *anj_service_provider_find_entry(int provider_type)
{
    anj_service_provider_entry_t *entry = s_stProviderList;

    while (entry)
    {
        if (entry->ops && entry->ops->provider_type == provider_type)
            return entry;
        entry = entry->next;
    }

    return NULL;
}

int anj_service_provider_register(const anj_service_provider_ops *ops)
{
    anj_service_provider_entry_t *entry = NULL;
    anj_service_provider_entry_t *current = NULL;

    if (!ops || !ops->provider_name)
        return -1;

    if (anj_service_provider_find_entry(ops->provider_type))
    {
        __WARN("service provider %s already registered\n", ops->provider_name);
        return 0;
    }

    entry = (anj_service_provider_entry_t *)malloc(sizeof(*entry));
    if (!entry)
        return -1;

    memset(entry, 0, sizeof(*entry));
    entry->ops = ops;

    if (!s_stProviderList || ops->provider_priority < s_stProviderList->ops->provider_priority)
    {
        entry->next = s_stProviderList;
        s_stProviderList = entry;
        return 0;
    }

    current = s_stProviderList;
    while (current->next && ops->provider_priority >= current->next->ops->provider_priority)
    {
        current = current->next;
    }

    entry->next = current->next;
    current->next = entry;
    return 0;
}

void anj_service_provider_unregister(const anj_service_provider_ops *ops)
{
    anj_service_provider_entry_t *prev = NULL;
    anj_service_provider_entry_t *current = s_stProviderList;

    while (current)
    {
        if (current->ops == ops)
        {
            if (prev)
                prev->next = current->next;
            else
                s_stProviderList = current->next;

            free(current);
            return;
        }

        prev = current;
        current = current->next;
    }
}

int anj_service_provider_init_all(void)
{
    anj_service_provider_entry_t *entry = s_stProviderList;
    int ret = 0;

    while (entry)
    {
        if (!entry->initialized && entry->ops && entry->ops->init)
        {
            __INFO("service provider init:%s\n", entry->ops->provider_name);
            ret = entry->ops->init();
            if (ret == 0)
                entry->initialized = 1;
        }

        entry = entry->next;
    }

    return ret;
}

int anj_service_provider_uninit_single(int provider_type)
{
    anj_service_provider_entry_t *entry = anj_service_provider_find_entry(provider_type);
    int ret = 0;

    if (!entry || !entry->initialized || !entry->ops || !entry->ops->uninit)
        return 0;

    __INFO("service provider uninit:%s\n", entry->ops->provider_name);
    ret = entry->ops->uninit();
    if (ret == 0)
        entry->initialized = 0;

    return ret;
}

int anj_service_provider_uninit_all(void)
{
    anj_service_provider_entry_t *entry = s_stProviderList;
    anj_service_provider_entry_t *entries[16] = {0};
    int count = 0;
    int ret = 0;
    int i = 0;

    while (entry && count < (int)(sizeof(entries) / sizeof(entries[0])))
    {
        entries[count++] = entry;
        entry = entry->next;
    }

    for (i = count - 1; i >= 0; --i)
    {
        entry = entries[i];
        if (!entry->initialized || !entry->ops || !entry->ops->uninit)
            continue;

        __INFO("service provider uninit:%s\n", entry->ops->provider_name);
        ret = entry->ops->uninit();
        if (ret == 0)
            entry->initialized = 0;
    }

    return ret;
}

int anj_service_provider_alarm_event_notify(void *alarm_event)
{
    anj_service_provider_entry_t *entry = s_stProviderList;
    int ret = 0;

    while (entry)
    {
        if (entry->ops && entry->ops->alarm_event_notify)
            ret = entry->ops->alarm_event_notify(alarm_event);
        entry = entry->next;
    }

    return ret;
}

int anj_service_provider_audio_enc_change(void)
{
    anj_service_provider_entry_t *entry = s_stProviderList;
    int ret = 0;

    while (entry)
    {
        if (entry->ops && entry->ops->audio_enc_change)
            ret = entry->ops->audio_enc_change();
        entry = entry->next;
    }

    return ret;
}

int anj_service_provider_has_capability(unsigned int capability_flags)
{
    anj_service_provider_entry_t *entry = s_stProviderList;

    while (entry)
    {
        if (entry->ops && (entry->ops->capability_flags & capability_flags) == capability_flags)
            return 1;
        entry = entry->next;
    }

    return 0;
}

static const anj_service_provider_ops *anj_service_provider_get_ops(int provider_type)
{
    anj_service_provider_entry_t *entry = anj_service_provider_find_entry(provider_type);

    if (!entry)
        return NULL;

    return entry->ops;
}

int anj_ftpemail_ftp_file(FtpServer *param, const char *path, const char *filename,
                          const char *remotepath, const char *remotefile)
{
    const anj_service_provider_ops *ops = anj_service_provider_get_ops(ANJ_SERVICE_PROVIDER_FTP_EMAIL);

    if (!ops || !ops->ftp_file)
        return -1;

    return ops->ftp_file(param, path, filename, remotepath, remotefile);
}

int anj_ftpemail_email_file(SmtpServerList *param, int account_index,
                            const char *path, const char *filename, const char *infomation)
{
    const anj_service_provider_ops *ops = anj_service_provider_get_ops(ANJ_SERVICE_PROVIDER_FTP_EMAIL);

    if (!ops || !ops->email_file)
        return -1;

    return ops->email_file(param, account_index, path, filename, infomation);
}

int anj_ftpemail_ftp_test(FtpServer *param, const char *device_ip)
{
    const anj_service_provider_ops *ops = anj_service_provider_get_ops(ANJ_SERVICE_PROVIDER_FTP_EMAIL);

    if (!ops || !ops->ftp_test)
        return -1;

    return ops->ftp_test(param, device_ip);
}

int anj_ftpemail_smtp_test(SmtpServerList *param, int account_index, const char *device_ip)
{
    const anj_service_provider_ops *ops = anj_service_provider_get_ops(ANJ_SERVICE_PROVIDER_FTP_EMAIL);

    if (!ops || !ops->smtp_test)
        return -1;

    return ops->smtp_test(param, account_index, device_ip);
}
