#ifndef __ANJ_SNAP_PROVIDER_H__
#define __ANJ_SNAP_PROVIDER_H__

#include "anj_snap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    int (*init)(void);
    int (*uninit)(void);
    int (*start_yuv)(int cam);
    int (*stop_yuv)(int cam);
    int (*prepare)(int cam, int stream, int *width, int *height, AreaStruct *are, int has_are);
    int (*capture)(int cam, int stream, int quality, const char *file, AreaStruct *are,
                   unsigned char *yuv, int width, int height);
} anj_snap_provider_ops;

int anj_snap_provider_register(const anj_snap_provider_ops *ops);
void anj_snap_provider_unregister(const anj_snap_provider_ops *ops);
const anj_snap_provider_ops *anj_snap_provider_get(void);
int anj_snap_provider_init(void);
int anj_snap_provider_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
