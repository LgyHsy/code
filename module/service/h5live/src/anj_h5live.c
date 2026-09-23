#include <unistd.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_config.h"
#include "anj_net.h"
#include "eventhub.h"
#include "anj_service_provider.h"
#include "anj_h5live.h"
#include "anj_h5_ws_server.h"
#include "anj_h5_stream.h"
#include "anj_h5_media.h"

static anj_thread_s s_h5live_ctrl_thread;
static anj_thread_s s_h5_restart_thread;
static int s_h5live_started = 0;

static int anj_h5live_audio_enc_change(void)
{
    anj_h5_audio_config_sync();
    anj_h5_live_audio_reader_restart();
    return 0;
}

static const anj_service_provider_ops s_stH5liveProviderOps = {
    .provider_name = "h5live",
    .provider_type = ANJ_SERVICE_PROVIDER_H5LIVE,
    .provider_priority = ANJ_SERVICE_PROVIDER_H5LIVE,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_NONE,
    .init = anj_h5live_init,
    .uninit = anj_h5live_uninit,
    .alarm_event_notify = NULL,
    .audio_enc_change = anj_h5live_audio_enc_change,
};

ANJ_LINK_KEEP(anj_keep_h5live_provider);

__attribute__((constructor)) static void anj_h5live_provider_register(void)
{
    anj_service_provider_register(&s_stH5liveProviderOps);
}

__attribute__((destructor)) static void anj_h5live_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stH5liveProviderOps);
}

static int anj_h5live_start_services(void)
{
    if (s_h5live_started)
    {
        return 0;
    }

    initStreamBuffers();
    anj_h5_audio_config_sync();
    anj_h5_media_monitor_start();
    if (anj_h5_ws_server_start() != 0)
    {
        anj_h5_media_monitor_stop();
        freeStreamBuffers();
        return -1;
    }

    s_h5live_started = 1;
    __INFO("h5live services started\n");
    return 0;
}

static void anj_h5live_stop_services(void)
{
    if (!s_h5live_started)
    {
        return;
    }

    anj_h5_ws_server_stop();
    anj_h5_media_monitor_stop();
    freeStreamBuffers();
    s_h5live_started = 0;
    __INFO("h5live services stopped\n");
}

static int anj_h5live_ctrl_thread(void *ctx, int *bStart)
{
    (void)ctx;

    while (bStart && *bStart)
    {
        MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
        if (pStreamCfg == NULL || pStreamCfg->webConfig.enable_web != 1)
        {
            usleep(200 * 1000);
            continue;
        }

        if (anj_net_wire_and_wireless_ip_ready_check())
        {
            break;
        }
        usleep(200 * 1000);
    }

    if (bStart && *bStart)
    {
        return anj_h5live_start_services();
    }

    return 0;
}

static int anj_h5live_restart_thread(void *ctx, int *bStart)
{
    (void)ctx;
    (void)bStart;

    anj_h5live_stop_services();
    sleep(1);
    anj_h5live_start_services();
    return 0;
}

static void anj_h5live_restart(EventResult *event_result, void *data)
{
    (void)data;

    if (event_result)
    {
        event_result->ret = 0;
    }

    if (s_h5_restart_thread.start != 0 && s_h5_restart_thread.end == 0)
    {
        __WARN("h5live restarting...\n");
        return;
    }

    memset(&s_h5_restart_thread, 0, sizeof(s_h5_restart_thread));
    s_h5_restart_thread.bAutoDestroy = 1;
    strncpy(s_h5_restart_thread.iThreadName, "h5_restart", sizeof(s_h5_restart_thread.iThreadName) - 1);
    s_h5_restart_thread.iThreadjob.ctx = NULL;
    s_h5_restart_thread.iThreadjob.func = anj_h5live_restart_thread;
    anj_thread_task_create(&s_h5_restart_thread);
}

int anj_h5live_init(void)
{
    __INFO("init h5live\n");

    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_H5_RESTART, anj_h5live_restart);

    memset(&s_h5live_ctrl_thread, 0, sizeof(s_h5live_ctrl_thread));
    s_h5live_ctrl_thread.bAutoDestroy = 1;
    strncpy(s_h5live_ctrl_thread.iThreadName, "h5live_ctrl", sizeof(s_h5live_ctrl_thread.iThreadName) - 1);
    s_h5live_ctrl_thread.iThreadjob.ctx = NULL;
    s_h5live_ctrl_thread.iThreadjob.func = anj_h5live_ctrl_thread;
    return anj_thread_task_create(&s_h5live_ctrl_thread);
}

int anj_h5live_uninit(void)
{
    __INFO("uninit h5live\n");

    anj_thread_task_destroy(&s_h5live_ctrl_thread, -1);
    anj_h5live_stop_services();

    __INFO("h5live uninit success\n");
    return 0;
}
