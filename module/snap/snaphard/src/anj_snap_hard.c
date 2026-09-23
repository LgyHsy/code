#include "anj_mw_comm.h"
#include "anj_mw_media_video.h"
#include "anj_snap_provider.h"

static int anj_snap_hard_init(void)
{
    int s32Ret = 0;
    for (int iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        s32Ret = anj_mw_media_video_jpg_init(iCameraIdx);
        if (s32Ret != 0)
        {
            __ERR("snap hard init failed cam=%d\n", iCameraIdx);
            return s32Ret;
        }
    }
    return s32Ret;
}

static int anj_snap_hard_uninit(void)
{
    int s32Ret = 0;
    for (int iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
    {
        s32Ret = anj_mw_media_video_jpg_uninit(iCameraIdx);
        if (s32Ret != 0)
        {
            __ERR("snap hard uninit failed cam=%d\n", iCameraIdx);
            return s32Ret;
        }
    }
    return s32Ret;
}

static int anj_snap_hard_capture(int cam, int stream, int quality, const char *file, AreaStruct *are,
                                 unsigned char *yuv, int width, int height)
{
    (void)are;
    (void)yuv;
    (void)width;
    (void)height;

    if (file == NULL)
    {
        __ERR("file is NULL\n");
        return -1;
    }
    if (anj_mw_media_video_jpg_capture(cam, stream, quality, file) == 0)
    {
        return 1;
    }
    return 0;
}

static const anj_snap_provider_ops s_stSnapHardProviderOps = {
    .provider_name = "snaphard",
    .provider_priority = 100,
    .init = anj_snap_hard_init,
    .uninit = anj_snap_hard_uninit,
    .start_yuv = 0,
    .stop_yuv = 0,
    .prepare = 0,
    .capture = anj_snap_hard_capture,
};

ANJ_LINK_KEEP(anj_keep_snap_hard_provider);

__attribute__((constructor)) static void anj_snap_hard_provider_register(void)
{
    anj_snap_provider_register(&s_stSnapHardProviderOps);
}

__attribute__((destructor)) static void anj_snap_hard_provider_unregister(void)
{
    anj_snap_provider_unregister(&s_stSnapHardProviderOps);
}
