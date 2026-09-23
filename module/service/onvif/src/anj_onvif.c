/** ===========================================================================

* Copyright (c) Anjoy Vision Information Technology Co.,Ltd
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied
*
* ===========================================================================
*/

/**
 * Copyright (C) by Anjoy Vision Information Company
 *
 * @File Name    : anj_onvif.c
 * @Description  :
 */

#include <unistd.h>
#include <string.h>

#include "anj_onvif.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_service_provider.h"
#include "anj_config.h"
#include "alarm_link.h"
#include "onvif_srv.h"
#include "onvif.h"
#include "onvif_notify.h"
#include "para.h"
#include "eventhub.h"

static int s_stOnvifInit = 0;
static anj_thread_s s_AnjOnvifThread;

static const anj_service_provider_ops s_stOnvifProviderOps = {
    .provider_name = "onvif",
    .provider_type = ANJ_SERVICE_PROVIDER_ONVIF,
    .provider_priority = ANJ_SERVICE_PROVIDER_ONVIF,
    .capability_flags = ANJ_SERVICE_PROVIDER_CAP_NONE,
    .init = anj_onvif_init,
    .uninit = anj_onvif_uninit,
    .alarm_event_notify = anj_onvif_alarm_event_notify,
    .audio_enc_change = NULL,
};

ANJ_LINK_KEEP(anj_keep_onvif_provider);

__attribute__((constructor)) static void anj_onvif_provider_register(void)
{
    anj_service_provider_register(&s_stOnvifProviderOps);
}

__attribute__((destructor)) static void anj_onvif_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stOnvifProviderOps);
}

static int anj_onvif_start_thread(void *ctx, int *bStart)
{
    (void)ctx;

    if (!bStart || *bStart == 0)
    {
        return 0;
    }

    onvif_start();

    MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (pStreamCfg != NULL && pStreamCfg->webConfig.enable_onvif > 0
        && g_onvif_cls.http_srv.sfd > 0)
    {
        s_stOnvifInit = 1;
        __INFO("onvif start done\n");
    }
    else
    {
        __ERR("onvif start failed: onvif=%d web=%d http_srv.sfd=%d\n",
              pStreamCfg ? pStreamCfg->webConfig.enable_onvif : 0,
              pStreamCfg ? pStreamCfg->webConfig.enable_web : 0,
              g_onvif_cls.http_srv.sfd);
    }

    return 0;
}

static int anj_onvif_start(void)
{
    MediaStreamConfig *pStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();
    if (pStreamCfg == NULL || pStreamCfg->webConfig.enable_onvif == 0)
    {
        __INFO("skip onvif provider start: onvif=%d web=%d\n",
               pStreamCfg ? pStreamCfg->webConfig.enable_onvif : 0,
               pStreamCfg ? pStreamCfg->webConfig.enable_web : 0);
        return 0;
    }

    if (s_stOnvifInit || s_AnjOnvifThread.start)
    {
        __ERR("had been started!\n");
        return 0;
    }

    memset(&s_AnjOnvifThread, 0, sizeof(s_AnjOnvifThread));
    s_AnjOnvifThread.bAutoDestroy = 0;
    strncpy(s_AnjOnvifThread.iThreadName, "onvif_start", sizeof(s_AnjOnvifThread.iThreadName) - 1);
    s_AnjOnvifThread.iThreadjob.ctx = &s_AnjOnvifThread;
    s_AnjOnvifThread.iThreadjob.func = anj_onvif_start_thread;

    if (anj_thread_task_create(&s_AnjOnvifThread) != 0)
    {
        __ERR("create onvif_start thread failed\n");
        return -1;
    }

    return 0;
}

static int anj_onvif_stop(void)
{
    if (!s_stOnvifInit && !s_AnjOnvifThread.start)
    {
        return 0;
    }

    if (s_AnjOnvifThread.start)
    {
        anj_thread_task_destroy(&s_AnjOnvifThread, 0);
        memset(&s_AnjOnvifThread, 0, sizeof(s_AnjOnvifThread));
    }

    if (s_stOnvifInit)
    {
        s_stOnvifInit = 0;
        onvif_stop();
    }

    return 0;
}

static void anj_onvif_restart(EventResult *event_result, void *data)
{
    (void)data;
    if (event_result)
    {
        event_result->ret = 0;
    }

    __INFO("onvif restart. \n");

    anj_onvif_stop();
    usleep(500 * 1000);
    anj_onvif_start();
}

int anj_onvif_init()
{
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_ONVIF_RESTART, anj_onvif_restart);
    return anj_onvif_start();
}

int anj_onvif_uninit()
{
    anj_onvif_stop();
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, EVENTHUB_ONVIF_RESTART, anj_onvif_restart);
    return 0;
}

int anj_onvif_alarm_event_notify(void *alarm_event)
{
    if (s_stOnvifInit == 0)
    {
        return 0;
    }
    ALARM_MSG_DATA alarm;

    if (alarm_event == NULL)
    {
        __ERR("alarm_event is NULL.\n");
        return -1;
    }

    alarm_event_data *event_data = (alarm_event_data *)alarm_event;

    memset(&alarm, 0, sizeof(alarm));
    alarm.alarmtime.year   = event_data->year;
    alarm.alarmtime.month  = event_data->month;
    alarm.alarmtime.day    = event_data->day;
    alarm.alarmtime.hour   = event_data->hour;
    alarm.alarmtime.minute = event_data->minute;
    alarm.alarmtime.second = event_data->second;
    alarm.alarmcode  = event_data->alarm_code;
    alarm.alarmflag  = event_data->alarm_flag;
    alarm.alarmlevel = event_data->alarm_level;
    snprintf(alarm.alarmdata, sizeof(alarm.alarmdata), "%s", event_data->alarm_payload);
    snprintf(alarm.snapfile, sizeof(alarm.snapfile), "%s", event_data->snap_path);

    __DBG("alarm event to onvif, time:%04d-%02d-%02d %02d:%02d:%02d, "
        "alarmcode:%d, alarmflag:%d, alarmlevel:%d\n",
        alarm.alarmtime.year, alarm.alarmtime.month, alarm.alarmtime.day,
        alarm.alarmtime.hour, alarm.alarmtime.minute, alarm.alarmtime.second,
        alarm.alarmcode, alarm.alarmflag, alarm.alarmlevel);

    return onvif_alarm_event_handle(&alarm);
}
