#ifndef __ANJ_GYRO_PROVIDER_H__
#define __ANJ_GYRO_PROVIDER_H__

#include "anj_gyro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    int sample_interval_ms;
    int (*init)(void);
    int (*uninit)(void);
    int (*read)(anj_gyro_data_t *out);
} anj_gyro_provider_ops;

int anj_gyro_provider_register(const anj_gyro_provider_ops *ops);
void anj_gyro_provider_unregister(const anj_gyro_provider_ops *ops);
int anj_gyro_provider_init(void);
int anj_gyro_provider_uninit(void);
int anj_gyro_provider_read(anj_gyro_data_t *out);
int anj_gyro_provider_sample_interval_ms(void);

#ifdef __cplusplus
}
#endif

#endif
