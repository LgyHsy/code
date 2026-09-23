#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <getopt.h>

#include "anj_mw_log.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mbuf.h"
#include "anj_config.h"
#include "eventhub.h"

#include "wis_streamer.hh"
#include "BasicUsageEnvironment.hh"
#include "Err.hh"
#include "rtsp_func_macro.hh"
#include "WISH264VideoServerMediaSubsession.hh"
#include "WISH265VideoServerMediaSubsession.hh"
#include "WISPCMAudioServerMediaSubsession.hh"
#include "MediaStreamInput.hh"
#include "ServerMediaSession.hh"
#include "RTSPServer.hh"

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

ANJ_LINK_KEEP(anj_keep_rtsp_provider);

__attribute__((constructor)) static void anj_rtsp_provider_register(void)
{
    anj_service_provider_register(&s_stRtspProviderOps);
}

__attribute__((destructor)) static void anj_rtsp_provider_unregister(void)
{
    anj_service_provider_unregister(&s_stRtspProviderOps);
}

/*
    多目产品上 各sensor的主码流配置相同，子码流配置相同，这里可以不用总编码通道数量
*/
#define RTSP_VIDEO_MAX_CHN MAX_VENC_CHN

typedef struct
{
    int enable;
    int type;
    int width;
    int height;
    int framerate;
    int bitrate;
} rtsp_video_param_t;

typedef struct
{
    int enable;
    int type;
} rtsp_audio_param_t;

typedef struct
{
    int rtsp_port;
    int rtsp_enable;
    int auth_enable;
    rtsp_audio_param_t audio_param;
    rtsp_video_param_t video_param[RTSP_VIDEO_MAX_CHN];
} AnjRtspConfig_t;

static anj_thread_s s_stRtspCtrlThread;
static AnjRtspConfig_t s_stAnjRtspConfig;

char const *MediaMainStreamName = "mpeg4";
char const *MediaSubstreamName = "mpeg4cif";
char const *streamDescription = "RTSP/RTP stream from Network Video Server";

RTSPServer *g_pRtspServer = NULL;

pthread_mutex_t rtsp_lock;

static anj_thread_s s_stRtspRestartThread = {0};
static void anj_rtsp_restart(EventResult *event_result, void *data);

void rtsp_close_client()
{
    pthread_mutex_lock(&rtsp_lock);
    RTSPServer *pstRtspServer = g_pRtspServer;
    pthread_mutex_unlock(&rtsp_lock);

    if (NULL == pstRtspServer)
    {
        return;
    }

    pstRtspServer->closeAllClientSession();
    return;
}

void rtsp_data_release(MediaStreamInput *MediaMainInputDevice[], MediaStreamInput *MediaSubInputDevice[], int camera_nums,
                       UserAuthenticationDatabase *authDB, RTSPServer *rtspServer,
                       TaskScheduler *scheduler, UsageEnvironment *env)
{
    int i = 0;
    for (i = 0; i < camera_nums; i++)
    {
        if (MediaMainInputDevice[i] != NULL)
        {
            Medium::close(MediaMainInputDevice[i]);
        }

        if (MediaSubInputDevice[i] != NULL)
        {
            Medium::close(MediaSubInputDevice[i]);
        }
    }

    if (authDB != NULL)
    {
        delete authDB;
    }

    if (rtspServer != NULL)
    {
        Medium::close(rtspServer);
    }

    if (env != NULL)
    {
        env->reclaim();
    }

    if (scheduler != NULL)
    {
        delete scheduler;
    }
    return;
}

void anj_rtsp_config_init()
{
    memset(&s_stAnjRtspConfig, 0, sizeof(s_stAnjRtspConfig));
    MediaStreamConfig *mediastreamcfg = (MediaStreamConfig *)getMediaStreamConfig();
    s_stAnjRtspConfig.rtsp_enable = mediastreamcfg->rtspConfig.enable_rtsp;
    s_stAnjRtspConfig.rtsp_port = mediastreamcfg->rtspConfig.videoPort;
    s_stAnjRtspConfig.auth_enable = mediastreamcfg->rtspConfig.rtsp_auth;

    MediaConfig *mediacfg = (MediaConfig *)getMediaConfig();
    AudioConfig *audiocfg = &mediacfg->audioConfig;
    VideoConfig *videocfg = &mediacfg->videoConfig[0];

    s_stAnjRtspConfig.audio_param.enable = audiocfg->audioEncode.enable;
    // s_stAnjRtspConfig.audio_param.enable = 0;
    if (strstr(audiocfg->audioEncode.audioEncodeType.typeName, "PCMU") == 0)
    {
        s_stAnjRtspConfig.audio_param.type = 0;
    }
    else
    {
        s_stAnjRtspConfig.audio_param.type = 1;
    }

    __INFO("rtsp audo enable:%d, type:%d\n", s_stAnjRtspConfig.audio_param.enable, s_stAnjRtspConfig.audio_param.type);

    int i = 0;
    ANJ_SIZE_S VideoSize = {0};
    for (i = 0; i < RTSP_VIDEO_MAX_CHN; i++)
    {
        s_stAnjRtspConfig.video_param[i].enable = videocfg->videoEncode.encodeCfg[i].enable;

        if (strstr(videocfg->videoEncode.encodeCfg[i].encodeFormat.name, "H265"))
        {
            if (0 == i)
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H265;
            }
            else
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H265_CIF;
            }
        }
        else if (strstr(videocfg->videoEncode.encodeCfg[i].encodeFormat.name, "H264"))
        {
            if (0 == i)
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H264;
            }
            else
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H264_CIF;
            }
        }
        else
        {
            __ERR("error encodeFormat name:%s, default use h264\n", videocfg->videoEncode.encodeCfg[i].encodeFormat.name);
            if (0 == i)
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H264;
            }
            else
            {
                s_stAnjRtspConfig.video_param[i].type = VIDEO_TYPE_H264_CIF;
            }
        }

        VideoSize =
            getPicSize(videocfg->videoEncode.encodeCfg[i].resolution.name, videocfg->videoCapture.tvsystem, videocfg->videoCapture.rotate, 0);
        s_stAnjRtspConfig.video_param[i].width = VideoSize.u32Width;
        s_stAnjRtspConfig.video_param[i].height = VideoSize.u32Height;
        s_stAnjRtspConfig.video_param[i].bitrate = videocfg->videoEncode.encodeCfg[i].bitRate;
        s_stAnjRtspConfig.video_param[i].framerate = videocfg->videoEncode.encodeCfg[i].frameRate;

        __INFO("rtsp video chn:%d enable:%d, type:%d, width:%d, height:%d, bitrate:%d, framerate:%d\n",
               i, s_stAnjRtspConfig.video_param[i].enable, s_stAnjRtspConfig.video_param[i].type,
               s_stAnjRtspConfig.video_param[i].width, s_stAnjRtspConfig.video_param[i].height,
               s_stAnjRtspConfig.video_param[i].bitrate, s_stAnjRtspConfig.video_param[i].framerate);
    }
}

static int anj_rtsp_thread(void *ctx, int *bStart)
{
    anj_thread_s *pThreadSelf = (anj_thread_s *)ctx;

    int index = 0;
    int iRetryTimes = 0;
    int iServerInitFailed = 0;

    MediaStreamInput *MediaMainInputDevice[ANJ_CAMERA_MAX_NUMS] = {NULL};
    MediaStreamInput *MediaSubInputDevice[ANJ_CAMERA_MAX_NUMS] = {NULL};
    UserAuthenticationDatabase *authDB = NULL;
    RTSPServer *rtspServer = NULL;
    TaskScheduler *scheduler = NULL;
    UsageEnvironment *env = NULL;

    __INFO("enter rtsp thread, support camera:%d\n", ANJ_CAMERA_MAX_NUMS);

    while (bStart && *bStart)
    {
        if (iRetryTimes >= 20)
        {
            __ERR("iRetryTimes:%d reach max!\n", iRetryTimes);
            break;
        }

        scheduler = BasicTaskScheduler::createNew();
        env = BasicUsageEnvironment::createNew(*scheduler);

        for (index = 0; index < ANJ_CAMERA_MAX_NUMS; index++)
        {
            if (s_stAnjRtspConfig.video_param[0].enable)
            {
                MediaMainInputDevice[index] = MediaStreamInput::createNew(*env, s_stAnjRtspConfig.video_param[0].type, 0, index);
                if (MediaMainInputDevice[index] == NULL)
                {
                    __ERR("Failed to create main MediaStream[%d] type:%d input device\n", index, s_stAnjRtspConfig.video_param[0].type);
                    goto FAILURE;
                }
            }

            if (s_stAnjRtspConfig.video_param[1].enable)
            {
                MediaSubInputDevice[index] = MediaStreamInput::createNew(*env, s_stAnjRtspConfig.video_param[1].type, 1, index);
                if (MediaSubInputDevice[index] == NULL)
                {
                    __ERR("Failed to create sub MediaStream[%d] type:%d input device\n", index, s_stAnjRtspConfig.video_param[1].type);
                    goto FAILURE;
                }
            }
        }

        if (s_stAnjRtspConfig.auth_enable)
        {
            authDB = new UserAuthenticationDatabase;
            if (authDB == NULL)
            {
                __ERR("new UserAuthenticationDatabase failed.\n");
                goto FAILURE;
            }
        }

        rtspServer = RTSPServer::createNew(*env, s_stAnjRtspConfig.rtsp_port, authDB, MAX_RTSP_SESSION_TIMEOUT, RTSP_MAX_VENC_CHN);
        if (rtspServer == NULL)
        {
            __ERR("Failed to create RTSP server: %s \n", env->getResultMsg());
            iServerInitFailed = 1;
            goto FAILURE;
        }
        else
        {
            iServerInitFailed = 0;
        }

        if (0 == iServerInitFailed) // 初始化完成就退出循环，等待live555自身的eventloop循环
        {
            break;
        }

    FAILURE:
        iRetryTimes++;

        rtsp_data_release(MediaMainInputDevice, MediaSubInputDevice, ANJ_CAMERA_MAX_NUMS, authDB, rtspServer, scheduler, env);
        usleep(100 * 1000);
    }

    int iInitSuccess = 0;
    if (0 == iServerInitFailed && iRetryTimes < 20)
    {
        iInitSuccess = 1;
    }

    __INFO("rtsp init iRetryTimes:%d, is success?%d\n", iRetryTimes, iInitSuccess);

    if (*bStart && 1 == iInitSuccess) // 如果初始化成功，并且线程仍需要运行，则继续执行
    {
        pthread_mutex_lock(&rtsp_lock);
        g_pRtspServer = rtspServer;
        pthread_mutex_unlock(&rtsp_lock);
        __INFO("...done initializing\n");

        char alias_idx0[16] = {0};
        char alias_idx1[16] = {0};
        char alias_idx2[16] = {0};
        char SessionName[64] = {0};

        /*
            根据老架构情况：
            单目设备url命名为：rtsp://192.168.69.101/stream0
            双目设备url命令为：rtsp://192.168.69.101/chn01/stream0 和 rtsp://192.168.69.101/chn02/stream0
        */
        // int max_camera_nums = ANJ_CAMERA_MAX_NUMS;

        if (s_stAnjRtspConfig.video_param[0].enable)
        {
            for (index = 0; index < ANJ_CAMERA_MAX_NUMS; index++)
            {
                if (ANJ_CAMERA_MAX_NUMS == 1)
                {
                    snprintf(SessionName, sizeof(SessionName), "%s", MediaMainStreamName);
                    if (s_stAnjRtspConfig.video_param[0].type == VIDEO_TYPE_H265)
                        snprintf(alias_idx0, sizeof(alias_idx0), "h265");
                    else
                        snprintf(alias_idx0, sizeof(alias_idx0), "h264");
                    snprintf(alias_idx1, sizeof(alias_idx1), "0");
                    snprintf(alias_idx2, sizeof(alias_idx2), "stream0");
                }
                else
                {
                    char chn_name[8] = {0};
                    snprintf(chn_name, sizeof(chn_name), "ch0%d", index + 1);

                    snprintf(SessionName, sizeof(SessionName), "%s/%s", chn_name, MediaMainStreamName);
                    if (s_stAnjRtspConfig.video_param[0].type == VIDEO_TYPE_H265)
                        snprintf(alias_idx0, sizeof(alias_idx0), "%s/h265", chn_name);
                    else
                        snprintf(alias_idx0, sizeof(alias_idx0), "%s/h264", chn_name);
                    snprintf(alias_idx1, sizeof(alias_idx1), "%s/0", chn_name);
                    snprintf(alias_idx2, sizeof(alias_idx2), "%s/stream0", chn_name);
                }

                ServerMediaSession *sms = ServerMediaSession::createNew(*env, (const char *)SessionName, (const char *)SessionName, streamDescription);

                sms->SetOtherStreamName(0, alias_idx0);
                sms->SetOtherStreamName(1, alias_idx1);
                sms->SetOtherStreamName(2, alias_idx2);

                if (s_stAnjRtspConfig.video_param[0].type == VIDEO_TYPE_H265)
                {
                    __INFO("Main Stream Type VIDEO_TYPE_H265\n");
                    sms->addSubsession(WISH265VideoServerMediaSubsession::createNew(sms->envir(),
                                                                                    *MediaMainInputDevice[index],
                                                                                    s_stAnjRtspConfig.video_param[0].bitrate,
                                                                                    s_stAnjRtspConfig.video_param[0].width,
                                                                                    s_stAnjRtspConfig.video_param[0].height,
                                                                                    s_stAnjRtspConfig.video_param[0].framerate));
                }
                else
                {
                    __INFO("Main Stream Type VIDEO_TYPE_H264\n");
                    sms->addSubsession(WISH264VideoServerMediaSubsession::createNew(sms->envir(),
                                                                                    *MediaMainInputDevice[index],
                                                                                    s_stAnjRtspConfig.video_param[0].bitrate,
                                                                                    s_stAnjRtspConfig.video_param[0].width,
                                                                                    s_stAnjRtspConfig.video_param[0].height,
                                                                                    s_stAnjRtspConfig.video_param[0].framerate));
                }

                __INFO("rtsp audio enable:%d, type:%d\n", s_stAnjRtspConfig.audio_param.enable, s_stAnjRtspConfig.audio_param.type);
                if (s_stAnjRtspConfig.audio_param.enable)
                {
                    if (s_stAnjRtspConfig.audio_param.type == 0)
                    {
                        sms->addSubsession(WISPCMAudioServerMediaSubsession::createNew(sms->envir(), *MediaMainInputDevice[index]));
                    }
                }

                rtspServer->addServerMediaSession(sms);
                char *url = rtspServer->rtspURL(sms);
                __INFO("anj main stream[%d] play url:%s, port:%d, auth:%d!\n", index, url, s_stAnjRtspConfig.rtsp_port, s_stAnjRtspConfig.auth_enable);
                delete[] url;
            }
        }

        // Create a record describing the media to be streamed:
        if (s_stAnjRtspConfig.video_param[1].enable)
        {
            for (index = 0; index < ANJ_CAMERA_MAX_NUMS; index++)
            {
                if (ANJ_CAMERA_MAX_NUMS == 1)
                {
                    snprintf(SessionName, sizeof(SessionName), "%s", MediaSubstreamName);
                    if (s_stAnjRtspConfig.video_param[0].type == VIDEO_TYPE_H265)
                        snprintf(alias_idx0, sizeof(alias_idx0), "h265cif");
                    else
                        snprintf(alias_idx0, sizeof(alias_idx0), "h264cif");
                    snprintf(alias_idx1, sizeof(alias_idx1), "1");
                    snprintf(alias_idx2, sizeof(alias_idx2), "stream1");
                }
                else
                {
                    char chn_name[8] = {0};
                    snprintf(chn_name, sizeof(chn_name), "ch0%d", index + 1);

                    snprintf(SessionName, sizeof(SessionName), "%s/%s", chn_name, MediaSubstreamName);
                    if (s_stAnjRtspConfig.video_param[0].type == VIDEO_TYPE_H265)
                        snprintf(alias_idx0, sizeof(alias_idx0), "%s/h265cif", chn_name);
                    else
                        snprintf(alias_idx0, sizeof(alias_idx0), "%s/h264cif", chn_name);
                    snprintf(alias_idx1, sizeof(alias_idx1), "%s/1", chn_name);
                    snprintf(alias_idx2, sizeof(alias_idx2), "%s/stream1", chn_name);
                }

                ServerMediaSession *sms = ServerMediaSession::createNew(*env, (const char *)SessionName, (const char *)SessionName, streamDescription);

                sms->SetOtherStreamName(0, alias_idx0);
                sms->SetOtherStreamName(1, alias_idx1);
                sms->SetOtherStreamName(2, alias_idx2);

                if (VIDEO_TYPE_H265_CIF == s_stAnjRtspConfig.video_param[1].type)
                {
                    __INFO("Sub Stream Type VIDEO_TYPE_H265\n");
                    sms->addSubsession(WISH265VideoServerMediaSubsession::createNew(sms->envir(),
                                                                                    *MediaSubInputDevice[index],
                                                                                    s_stAnjRtspConfig.video_param[1].bitrate,
                                                                                    s_stAnjRtspConfig.video_param[1].width,
                                                                                    s_stAnjRtspConfig.video_param[1].height,
                                                                                    s_stAnjRtspConfig.video_param[1].framerate));
                }
                else
                {
                    __INFO("Sub Stream Type VIDEO_TYPE_H264\n");
                    sms->addSubsession(WISH264VideoServerMediaSubsession::createNew(sms->envir(),
                                                                                    *MediaSubInputDevice[index],
                                                                                    s_stAnjRtspConfig.video_param[1].bitrate,
                                                                                    s_stAnjRtspConfig.video_param[1].width,
                                                                                    s_stAnjRtspConfig.video_param[1].height,
                                                                                    s_stAnjRtspConfig.video_param[1].framerate));
                }

                if (s_stAnjRtspConfig.audio_param.enable)
                {
                    if (s_stAnjRtspConfig.audio_param.type == 0)
                    {
                        sms->addSubsession(WISPCMAudioServerMediaSubsession::createNew(sms->envir(), *MediaSubInputDevice[index]));
                    }
                }

                rtspServer->addServerMediaSession(sms);
                char *url = rtspServer->rtspURL(sms);

                __INFO("anj sub stream[%d] play url:%s, port:%d, auth:%d\n", index, url, s_stAnjRtspConfig.rtsp_port, s_stAnjRtspConfig.auth_enable);
                delete[] url;
            }
        }

        env->taskScheduler().doEventLoop((char *)&pThreadSelf->start); // does not return
        __INFO("exit rtsp main loop %d.\n", pThreadSelf->start);
    }

    __INFO("exit rtsp thread\n");

    // 退出live555 或者销毁线程时，都需要释放资源
    rtsp_data_release(MediaMainInputDevice, MediaSubInputDevice, ANJ_CAMERA_MAX_NUMS, authDB, rtspServer, scheduler, env);

    for (index = 0; index < ANJ_CAMERA_MAX_NUMS; index++)
    {
        MediaMainInputDevice[index] = NULL;
        MediaSubInputDevice[index] = NULL;
    }
    authDB = NULL;
    rtspServer = NULL;
    scheduler = NULL;
    env = NULL;

    pthread_mutex_lock(&rtsp_lock);

    if (g_pRtspServer)
    {
        g_pRtspServer = NULL;
    }

    pthread_mutex_unlock(&rtsp_lock);

    return 0;
}

int anj_rtsp_init(void)
{
    int iRet = 0;

    __INFO("init RTSP \n");
    anj_rtsp_config_init();

    pthread_mutex_init(&rtsp_lock, NULL);

    memset(&s_stRtspCtrlThread, 0, sizeof(anj_thread_s));
    s_stRtspCtrlThread.bAutoDestroy = 0;
    strncpy(s_stRtspCtrlThread.iThreadName, "anj_rtsp_thread", sizeof(s_stRtspCtrlThread.iThreadName) - 1);
    s_stRtspCtrlThread.iThreadjob.ctx = &s_stRtspCtrlThread;
    s_stRtspCtrlThread.iThreadjob.func = anj_rtsp_thread;
    iRet = anj_thread_task_create(&s_stRtspCtrlThread);

    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_RTSP_RESTART, anj_rtsp_restart);

    return iRet;
}

int anj_rtsp_uninit(void)
{
    __INFO("uninit RTSP \n");
    rtsp_close_client();
    usleep(1000 * 1000);

    anj_thread_task_destroy(&s_stRtspCtrlThread, -1);

    pthread_mutex_destroy(&rtsp_lock);
    return 0;
}

static int anj_rtsp_restart_thread(void *ctx, int *bStart)
{
    anj_rtsp_uninit();
    sleep(1);

    anj_rtsp_init();
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
}
