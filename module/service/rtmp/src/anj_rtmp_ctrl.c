#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_mw_time.h"
#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_config.h"
#include "anj_net.h"
#include "anj_video.h"
#include "eventhub.h"
#include "anj_rtmp_internal.h"

static anj_rtmp_session_t s_rtmp_session;
static anj_thread_s s_rtmp_ctrl_thread;
static anj_thread_s s_rtmp_restart_thread;
static int s_rtmp_ctrl_running = 0;
static int s_rtmp_need_restart = 0;

static int rtmp_config_ready(const RtmpConfig *cfg)
{
    if (cfg == NULL || cfg->enable <= 0 || cfg->port <= 0)
    {
        return 0;
    }

    if (cfg->server[0] == '\0' || cfg->appname[0] == '\0' || cfg->streamid[0] == '\0')
    {
        return 0;
    }

    if (cfg->streamno < 0 || cfg->streamno > 1)
    {
        return 0;
    }

    return 1;
}

static int rtmp_type_from_cfg(const RtmpConfig *cfg)
{
    char file_ver[64] = {0};
    int rtmp_type = (cfg != NULL && cfg->type == 1) ? 1 : 0;

    if (read_file_to_string("/etc/filesys.ver", file_ver, sizeof(file_ver)) == 0 &&
        strstr(file_ver, "_Y_") != NULL)
    {
        rtmp_type = 1;
    }

    return rtmp_type;
}

static int rtmp_use_system_timestamp(void)
{
    return (anj_mw_file_exists("/opt/ch/flag.rtmp.ts.sys") == 0 ||
            anj_mw_file_exists("/tmp/flag.rtmp.ts.sys") == 0) ? 1 : 0;
}

static void anj_rtmp_services_stop(void)
{
    anj_rtmp_media_stop();
    anj_rtmp_session_stop(&s_rtmp_session);
    anj_rtmp_mux_reset_state(&s_rtmp_session);
}

static void anj_rtmp_session_cleanup(void)
{
    anj_rtmp_services_stop();
    if (s_rtmp_session.mutex_inited)
    {
        pthread_mutex_destroy(&s_rtmp_session.push_mutex);
        s_rtmp_session.mutex_inited = 0;
    }
    memset(&s_rtmp_session, 0, sizeof(s_rtmp_session));
}

static int anj_rtmp_services_start(const RtmpConfig *cfg)
{
    int width = 0;
    int height = 0;
    int framerate = 0;
    int audiodatarate = 0;
    int audiosamplerate = 0;

    memset(&s_rtmp_session, 0, sizeof(s_rtmp_session));
    pthread_mutex_init(&s_rtmp_session.push_mutex, NULL);
    s_rtmp_session.mutex_inited = 1;
    s_rtmp_session.use_system_ts = rtmp_use_system_timestamp();

    if (anj_rtmp_session_start(cfg, &s_rtmp_session) != 0)
    {
        anj_rtmp_session_cleanup();
        return -1;
    }

    s_rtmp_session.rtmp_type = rtmp_type_from_cfg(cfg);
    if (anj_rtmp_mux_fill_codec_info(&s_rtmp_session) != 0)
    {
        anj_rtmp_session_cleanup();
        return -1;
    }

    if (anj_rtmp_mux_get_av_param(&s_rtmp_session, &width, &height, &framerate,
                                  &audiodatarate, &audiosamplerate) != 0)
    {
        anj_rtmp_session_cleanup();
        return -1;
    }

    if (anj_rtmp_mux_publish_script(&s_rtmp_session, width, height, framerate,
                                    audiodatarate, audiosamplerate) != 0)
    {
        __ERR("rtmp publish script failed\n");
    }

    anj_rtmp_mux_publish_aac_header(&s_rtmp_session);
    anj_video_request_idr(0, s_rtmp_session.stream_no);

    if (anj_rtmp_media_start(&s_rtmp_session) != 0)
    {
        anj_rtmp_session_cleanup();
        return -1;
    }

    __INFO("rtmp services started, streamno=%d\n", s_rtmp_session.stream_no);
    return 0;
}

static int anj_rtmp_restart_thread(void *ctx, int *bStart)
{
    (void)ctx;
    (void)bStart;

    anj_rtmp_session_cleanup();
    sleep(2);
    s_rtmp_need_restart = 1;
    return 0;
}

static void anj_rtmp_on_restart(EventResult *event_result, void *data)
{
    (void)data;

    if (event_result)
    {
        event_result->ret = 0;
    }

    if (s_rtmp_restart_thread.start != 0 && s_rtmp_restart_thread.end == 0)
    {
        __WARN("rtmp restarting...\n");
        return;
    }

    memset(&s_rtmp_restart_thread, 0, sizeof(s_rtmp_restart_thread));
    s_rtmp_restart_thread.bAutoDestroy = 1;
    strncpy(s_rtmp_restart_thread.iThreadName, "rtmp_restart", sizeof(s_rtmp_restart_thread.iThreadName) - 1);
    s_rtmp_restart_thread.iThreadjob.ctx = NULL;
    s_rtmp_restart_thread.iThreadjob.func = anj_rtmp_restart_thread;
    anj_thread_task_create(&s_rtmp_restart_thread);
}

void anj_rtmp_ctrl_restart(void)
{
    anj_rtmp_on_restart(NULL, NULL);
}

static int anj_rtmp_ctrl_thread(void *ctx, int *bStart)
{
    int publish_running = 0;
    unsigned int loop_count = 0;

    (void)ctx;

    while (bStart && *bStart && s_rtmp_ctrl_running)
    {
        MediaStreamConfig *stream_cfg = (MediaStreamConfig *)getMediaStreamConfig();
        RtmpConfig *rtmp_cfg = stream_cfg ? &stream_cfg->rtmpConfig : NULL;
        int in_timespan = 1;
        int should_publish = 0;

        if (s_rtmp_need_restart)
        {
            publish_running = 0;
            s_rtmp_need_restart = 0;
        }

        if (rtmp_cfg != NULL && rtmp_config_ready(rtmp_cfg))
        {
            in_timespan = CheckNowIsInTimeSpan(&rtmp_cfg->timeSpan);
            should_publish = in_timespan;
        }

        if (should_publish && !publish_running)
        {
            while (bStart && *bStart && s_rtmp_ctrl_running)
            {
                if (anj_net_wire_and_wireless_ip_ready_check())
                {
                    break;
                }
                usleep(200 * 1000);
            }

            if (bStart && *bStart && s_rtmp_ctrl_running && anj_rtmp_services_start(rtmp_cfg) == 0)
            {
                publish_running = 1;
            }
        }
        else if ((!should_publish || !rtmp_config_ready(rtmp_cfg)) && publish_running)
        {
            anj_rtmp_session_cleanup();
            publish_running = 0;
        }

        if (publish_running && ((loop_count++ % 100) == 0) && !CheckNowIsInTimeSpan(&rtmp_cfg->timeSpan))
        {
            __INFO("rtmp not in timespan, stop publish\n");
            anj_rtmp_session_cleanup();
            publish_running = 0;
        }

        usleep(100 * 1000);
    }

    anj_rtmp_session_cleanup();
    return 0;
}

int anj_rtmp_ctrl_start(void)
{
    if (s_rtmp_ctrl_running)
    {
        return 0;
    }

    s_rtmp_ctrl_running = 1;
    memset(&s_rtmp_ctrl_thread, 0, sizeof(s_rtmp_ctrl_thread));
    s_rtmp_ctrl_thread.bAutoDestroy = 1;
    strncpy(s_rtmp_ctrl_thread.iThreadName, "rtmp_ctrl", sizeof(s_rtmp_ctrl_thread.iThreadName) - 1);
    s_rtmp_ctrl_thread.iThreadjob.ctx = NULL;
    s_rtmp_ctrl_thread.iThreadjob.func = anj_rtmp_ctrl_thread;
    return anj_thread_task_create(&s_rtmp_ctrl_thread);
}

void anj_rtmp_ctrl_stop(void)
{
    s_rtmp_ctrl_running = 0;
    anj_thread_task_destroy(&s_rtmp_ctrl_thread, 0);
    anj_rtmp_session_cleanup();
    anj_rtmp_mux_uninit();
    memset(&s_rtmp_ctrl_thread, 0, sizeof(s_rtmp_ctrl_thread));
}
