#ifndef __ANJ_PTZ_PROVIDER_H__
#define __ANJ_PTZ_PROVIDER_H__

#include "anj_ptz.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    int (*init)(void);
    int (*uninit)(void);
    int (*operate)(int mode, int arg, int speed);
    void (*debug)(void);
    void (*dir_set)(PtzDir *pstPtzDir);
    void (*speed_set)(PtzSpeed *pstPtzSpeed);
} anj_ptz_provider_ops;

int anj_ptz_provider_register(const anj_ptz_provider_ops *ops);
void anj_ptz_provider_unregister(const anj_ptz_provider_ops *ops);
int anj_ptz_provider_available(void);
int anj_ptz_provider_init(void);
int anj_ptz_provider_uninit(void);
int anj_ptz_provider_operate(int mode, int arg, int speed);
void anj_ptz_provider_debug(void);
void anj_ptz_provider_dir_set(PtzDir *pstPtzDir);
void anj_ptz_provider_speed_set(PtzSpeed *pstPtzSpeed);

#ifdef __cplusplus
}
#endif

#endif
