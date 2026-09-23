#include <stddef.h>

#include "anj_mw_comm.h"
#include "anj_sysctl.h"
#include "function_list.h"
#include "anj_ser_provider.h"

static int anj_cloud_feature_init(void);
static int anj_cloud_feature_uninit(void);
static void anj_cloud_feature_check_support(int status);
static void anj_cloud_feature_binduser_check(char *pstBindUser);
static void anj_cloud_feature_push_alarm(int channel, int eventype, int buploadcloud);
static void anj_cloud_feature_push_video(int camera_type, int streamtype, int iskey,
                                         unsigned char *frameBuf, unsigned int frameLen);
static void anj_cloud_feature_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen);
static const anj_cloud_feature_ops *s_stCloudFeatureOps = NULL;

static const anj_cloud_ops s_stCloudOps = {
    .init = anj_cloud_feature_init,
    .uninit = anj_cloud_feature_uninit,
    .check_support = anj_cloud_feature_check_support,
    .binduser_check = anj_cloud_feature_binduser_check,
    .push_alarm = anj_cloud_feature_push_alarm,
    .push_video = anj_cloud_feature_push_video,
    .push_audio = anj_cloud_feature_push_audio,
};

ANJ_LINK_KEEP(anj_keep_cloud_provider);

__attribute__((constructor)) static void anj_cloud_provider_register(void)
{
    anj_cloud_ops_register(&s_stCloudOps);
}

__attribute__((destructor)) static void anj_cloud_provider_unregister(void)
{
    anj_cloud_ops_unregister(&s_stCloudOps);
}

int anj_cloud_feature_register(const anj_cloud_feature_ops *ops)
{
    if (!ops)
        return -1;

    s_stCloudFeatureOps = ops;
    return 0;
}

void anj_cloud_feature_unregister(const anj_cloud_feature_ops *ops)
{
    if (s_stCloudFeatureOps == ops)
        s_stCloudFeatureOps = NULL;
}

static int anj_cloud_feature_init(void)
{
    anj_sysctl_capability_add(FUNCTION_ALARM_PUSH_CLOUD);
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->init)
        return s_stCloudFeatureOps->init();
    return 0;
}

static int anj_cloud_feature_uninit(void)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->uninit)
        return s_stCloudFeatureOps->uninit();
    return 0;
}

static void anj_cloud_feature_check_support(int status)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->check_support)
        s_stCloudFeatureOps->check_support(status);
}

static void anj_cloud_feature_binduser_check(char *pstBindUser)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->binduser_check)
        s_stCloudFeatureOps->binduser_check(pstBindUser);
}

static void anj_cloud_feature_push_alarm(int channel, int eventype, int buploadcloud)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->push_alarm)
        s_stCloudFeatureOps->push_alarm(channel, eventype, buploadcloud);
}

static void anj_cloud_feature_push_video(int camera_type, int streamtype, int iskey,
                                         unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->push_video)
        s_stCloudFeatureOps->push_video(camera_type, streamtype, iskey, frameBuf, frameLen);
}

static void anj_cloud_feature_push_audio(int camera_type, unsigned char *frameBuf, unsigned int frameLen)
{
    if (s_stCloudFeatureOps && s_stCloudFeatureOps->push_audio)
        s_stCloudFeatureOps->push_audio(camera_type, frameBuf, frameLen);
}
