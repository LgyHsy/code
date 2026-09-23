#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_config.h"
#include "eventhub.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "anj_service_provider.h"
#include "anj_rtmp.h"
#include "anj_rtmp_internal.h"

static const anj_service_provider_ops s_stRtmpProviderOps = {
    "rtmp",
    ANJ_SERVICE_PROVIDER_RTMP,
    ANJ_SERVICE_PROVIDER_RTMP,
    ANJ_SERVICE_PROVIDER_CAP_NONE,
    anj_rtmp_init,
    anj_rtmp_uninit,
    NULL,
    NULL,
};

ANJ_LINK_KEEP(anj_keep_rtmp_provider);

__attribute__((constructor)) static void anj_rtmp_provider_register(void)
{
    anj_service_provider_register(&s_stRtmpProviderOps);
}

__attribute__((destructor)) static void anj_rtmp_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stRtmpProviderOps);
}

static void anj_rtmp_restart(EventResult *event_result, void *data)
{
    (void)data;
    anj_rtmp_ctrl_restart();
    if (event_result)
    {
        event_result->ret = 0;
    }
}

int anj_rtmp_init(void)
{
    __INFO("init RTMP\n");
    anj_sysctl_capability_add(FUNCTION_SUPPORT_RTMP);
    anj_sysctl_capability_add(FUNCTION_SUPPORT_RTMP_TIMESPAN);

    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_RTMP_RESTART, anj_rtmp_restart);
    return anj_rtmp_ctrl_start();
}

int anj_rtmp_uninit(void)
{
    __INFO("uninit RTMP\n");
    anj_rtmp_ctrl_stop();
    return 0;
}


