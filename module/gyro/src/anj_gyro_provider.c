#include "anj_gyro_provider.h"
#include "anj_mw_log.h"

static const anj_gyro_provider_ops *s_pstGyroProviderOps = 0;

int anj_gyro_provider_register(const anj_gyro_provider_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstGyroProviderOps == 0 ||
        ops->provider_priority >= s_pstGyroProviderOps->provider_priority)
    {
        s_pstGyroProviderOps = ops;
    }

    return 0;
}

void anj_gyro_provider_unregister(const anj_gyro_provider_ops *ops)
{
    if (s_pstGyroProviderOps == ops)
    {
        s_pstGyroProviderOps = 0;
    }
}

int anj_gyro_provider_init(void)
{
    if (s_pstGyroProviderOps && s_pstGyroProviderOps->init)
    {
        return s_pstGyroProviderOps->init();
    }

    __ERR("gyro provider not registered\n");
    return -1;
}

int anj_gyro_provider_uninit(void)
{
    if (s_pstGyroProviderOps && s_pstGyroProviderOps->uninit)
    {
        return s_pstGyroProviderOps->uninit();
    }

    return 0;
}

int anj_gyro_provider_read(anj_gyro_data_t *out)
{
    if (s_pstGyroProviderOps && s_pstGyroProviderOps->read)
    {
        return s_pstGyroProviderOps->read(out);
    }

    return -1;
}

int anj_gyro_provider_sample_interval_ms(void)
{
    if (s_pstGyroProviderOps && s_pstGyroProviderOps->sample_interval_ms > 0)
    {
        return s_pstGyroProviderOps->sample_interval_ms;
    }

    return 100;
}
