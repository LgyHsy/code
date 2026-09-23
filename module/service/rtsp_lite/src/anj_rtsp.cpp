#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <getopt.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mbuf.h"
#include "anj_config.h"
#include "eventhub.h"

#include "rtsp_server.h"
#include "anj_rtsp.h"
#include "anj_service_provider.h"

static const anj_service_provider_ops s_stRtspProviderOps = {
    "rtsp",
    ANJ_SERVICE_PROVIDER_RTSP,
    ANJ_SERVICE_PROVIDER_RTSP,
    ANJ_SERVICE_PROVIDER_CAP_NONE,
    anj_rtsp_init,
    anj_rtsp_uninit,
    NULL,
    NULL,
};

ANJ_LINK_KEEP(anj_keep_rtsp_lite_provider);

__attribute__((constructor)) static void anj_rtsp_provider_register(void)
{
    anj_service_provider_register(&s_stRtspProviderOps);
}

__attribute__((destructor)) static void anj_rtsp_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stRtspProviderOps);
}

static anj_thread_s s_stRtspRestartThread = {0};
static void anj_rtsp_restart(EventResult *event_result, void *data);

int anj_rtsp_init(void)
{
    __INFO("init RTSP LITE \n");
    rtsp_server_init();

    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_RTSP_RESTART, anj_rtsp_restart);
    return 0;
}

int anj_rtsp_uninit(void)
{
    __INFO("uninit RTSP LITE \n");
    rtsp_server_uninit();

    return 0;
}

static int anj_rtsp_restart_thread(void *ctx, int *bStart)
{
    rtsp_server_restart();

    rtsp_server_uninit();
    sleep(1);

    rtsp_server_init();
    return 0;
}

static void anj_rtsp_restart(EventResult *event_result, void *data)
{
    if (event_result)
    {
        event_result->ret = 0;
        if (s_stRtspRestartThread.start != 0 && s_stRtspRestartThread.end == 0)
        {
            __ERR("rtsp restarting...\n");
            return;
        }

        memset(&s_stRtspRestartThread, 0, sizeof(anj_thread_s));
        s_stRtspRestartThread.bAutoDestroy = 1;
        strncpy(s_stRtspRestartThread.iThreadName, "rtsp_restart", sizeof(s_stRtspRestartThread.iThreadName) - 1);
        s_stRtspRestartThread.iThreadjob.ctx = (void *)&s_stRtspRestartThread;
        s_stRtspRestartThread.iThreadjob.func = anj_rtsp_restart_thread;
        anj_thread_task_create(&s_stRtspRestartThread);
    }

    return;
}
