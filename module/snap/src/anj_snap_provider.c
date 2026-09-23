#include "anj_snap_provider.h"

static const anj_snap_provider_ops *s_pstSnapProviderOps = 0;

int anj_snap_provider_register(const anj_snap_provider_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstSnapProviderOps == 0 ||
        ops->provider_priority >= s_pstSnapProviderOps->provider_priority)
    {
        s_pstSnapProviderOps = ops;
    }

    return 0;
}

void anj_snap_provider_unregister(const anj_snap_provider_ops *ops)
{
    if (s_pstSnapProviderOps == ops)
    {
        s_pstSnapProviderOps = 0;
    }
}

const anj_snap_provider_ops *anj_snap_provider_get(void)
{
    return s_pstSnapProviderOps;
}

int anj_snap_provider_init(void)
{
    if (s_pstSnapProviderOps && s_pstSnapProviderOps->init)
    {
        return s_pstSnapProviderOps->init();
    }
    return 0;
}

int anj_snap_provider_uninit(void)
{
    if (s_pstSnapProviderOps && s_pstSnapProviderOps->uninit)
    {
        return s_pstSnapProviderOps->uninit();
    }
    return 0;
}
