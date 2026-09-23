#include "anj_mw_comm.h"
#include "anj_mw_media_video.h"
#include "anj_video.h"
#include "anj_smart.h"
#include "anj_snap_provider.h"
#include "jpg_encode.h"

#include <string.h>

static int anj_snap_soft_init(void)
{
    return 0;
}

static int anj_snap_soft_uninit(void)
{
    return 0;
}

static int anj_snap_soft_start_yuv(int cam)
{
    return anj_mw_media_video_jpg_start(cam, NULL);
}

static int anj_snap_soft_stop_yuv(int cam)
{
    return anj_mw_media_video_jpg_stop(cam);
}

static int anj_snap_soft_prepare(int cam, int stream, int *width, int *height, AreaStruct *are, int has_are)
{
    (void)stream;

    if (width == NULL || height == NULL || are == NULL)
    {
        return -1;
    }

    if (anj_smart_size_get(cam, width, height) != 0)
    {
        __ERR("anj_smart_size_get failed, cam:%d\n", cam);
        return -1;
    }
    

    if (!has_are)
    {
        int align_height = ANJ_ALIGN_UP(*height, 32);
        memset(are, 0, sizeof(*are));
        if (align_height != *height)
        {
            are->width = 100;
            are->height = (*height * 100) / align_height;
            are->height = ANJ_ALIGN_UP(are->height, 2);
        }
    }
    return 0;
}

static int anj_snap_soft_capture(int cam, int stream, int quality, const char *file, AreaStruct *are,
                                 unsigned char *yuv, int width, int height)
{
    int jpgsize = 0;
    int align_h;
    (void)cam;
    (void)stream;

    if (yuv == NULL || file == NULL || width <= 0 || height <= 0)
    {
        return 0;
    }

    align_h = ANJ_ALIGN_UP(height, 32);
    if (are == NULL || are->width == 0 || are->height == 0 ||
        (are->xPos == 0 && are->yPos == 0 && are->width == 100 && are->height == 100))
    {
        jpgsize = nv12_to_jpgfile(yuv, width, align_h, quality, file);
    }
    else
    {
        jpgsize = nv12_crop_to_jpgfile(yuv, width, align_h, *are, quality, file);
    }
    return jpgsize;
}

static const anj_snap_provider_ops s_stSnapSoftProviderOps = {
    .provider_name = "snapsoft",
    .provider_priority = 100,
    .init = anj_snap_soft_init,
    .uninit = anj_snap_soft_uninit,
    .start_yuv = anj_snap_soft_start_yuv,
    .stop_yuv = anj_snap_soft_stop_yuv,
    .prepare = anj_snap_soft_prepare,
    .capture = anj_snap_soft_capture,
};

ANJ_LINK_KEEP(anj_keep_snap_soft_provider);

__attribute__((constructor)) static void anj_snap_soft_provider_register(void)
{
    anj_snap_provider_register(&s_stSnapSoftProviderOps);
}

__attribute__((destructor)) static void anj_snap_soft_provider_unregister(void)
{
    anj_snap_provider_unregister(&s_stSnapSoftProviderOps);
}
