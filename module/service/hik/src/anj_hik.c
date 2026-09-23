#include <unistd.h>
#include <string.h>

#include "anj_hik.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_service_provider.h"
#include "anj_config.h"
#include "alarm_link.h"
#include "hik_sadp_glue.h"
#include "hik_net_server.h"
#include "hik_net_alarm.h"
#include "eventhub.h"
#include "anj_sysctl.h"
#include "function_list.h"

static int s_hik_inited = 0;
static anj_thread_s s_hik_start_thread;

static int anj_hik_alarm_event_notify(void *alarm_event)
{
    alarm_event_data *ev = (alarm_event_data *)alarm_event;

    if (ev == NULL)
    {
        return -1;
    }
    return hik_net_alarm_notify(ev->alarm_code, ev->alarm_flag);
}

static const anj_service_provider_ops s_stHikProviderOps = {
    .provider_name = "hik",
    .provider_type = ANJ_SERVICE_PROVIDER_HIK,
    .provider_priority = ANJ_SERVICE_PROVIDER_HIK,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_NONE,
    .init = anj_hik_init,
    .uninit = anj_hik_uninit,
    .alarm_event_notify = anj_hik_alarm_event_notify,
    .audio_enc_change = NULL,
};

ANJ_LINK_KEEP(anj_keep_hik_provider);

__attribute__((constructor)) static void anj_hik_provider_register(void)
{
    anj_service_provider_register(&s_stHikProviderOps);
}

__attribute__((destructor)) static void anj_hik_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stHikProviderOps);
}

static int anj_hik_start_thread(void *ctx, int *bStart)
{
    MediaStreamConfig *pStreamCfg = NULL;
    int ret = 0;

    (void)ctx;
    if (bStart == NULL || *bStart == 0)
    {
        return 0;
    }

    pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (pStreamCfg == NULL || pStreamCfg->hikConfig.enable == 0)
    {
        __INFO("hik start skipped: enable=0\n");
        return 0;
    }

    ret = hik_sadp_start();
    if (ret != 0)
    {
        __ERR("hik_sadp_start failed\n");
    }

    ret = hik_net_server_start(pStreamCfg->hikConfig.port);
    if (ret != 0)
    {
        __ERR("hik_net_server_start failed\n");
        hik_sadp_stop();
        return -1;
    }

    s_hik_inited = 1;
    __INFO("hik provider start done port=%u\n", pStreamCfg->hikConfig.port);
    return 0;
}

static int anj_hik_start(void)
{
    MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    if (pStreamCfg == NULL || pStreamCfg->hikConfig.enable == 0)
    {
        __INFO("skip hik provider start: enable=%d\n",
               pStreamCfg ? pStreamCfg->hikConfig.enable : 0);
        return 0;
    }

    if (s_hik_inited || s_hik_start_thread.start)
    {
        __ERR("hik had been started!\n");
        return 0;
    }

    anj_sysctl_capability_add(FUNCTION_HIK_CONFIG);
    
    memset(&s_hik_start_thread, 0, sizeof(s_hik_start_thread));
    s_hik_start_thread.bAutoDestroy = 0;
    strncpy(s_hik_start_thread.iThreadName, "hik_start", sizeof(s_hik_start_thread.iThreadName) - 1);
    s_hik_start_thread.iThreadjob.ctx = &s_hik_start_thread;
    s_hik_start_thread.iThreadjob.func = anj_hik_start_thread;

    if (anj_thread_task_create(&s_hik_start_thread) != 0)
    {
        __ERR("create hik_start thread failed\n");
        return -1;
    }
    return 0;
}

static int anj_hik_stop(void)
{
    if (!s_hik_inited && !s_hik_start_thread.start)
    {
        return 0;
    }

    if (s_hik_start_thread.start)
    {
        anj_thread_task_destroy(&s_hik_start_thread, 0);
        memset(&s_hik_start_thread, 0, sizeof(s_hik_start_thread));
    }

    hik_net_server_stop();
    hik_sadp_stop();
    s_hik_inited = 0;
    __INFO("hik provider stop done\n");
    return 0;
}

static void anj_hik_restart(EventResult *event_result, void *data)
{
    (void)data;
    if (event_result)
    {
        event_result->ret = 0;
    }

    __INFO("hik restart\n");
    anj_hik_stop();
    usleep(500 * 1000);
    anj_hik_start();
}

int anj_hik_init(void)
{
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_HIK_RESTART, anj_hik_restart);
    return anj_hik_start();
}

int anj_hik_uninit(void)
{
    anj_hik_stop();
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, EVENTHUB_HIK_RESTART, anj_hik_restart);
    return 0;
}
