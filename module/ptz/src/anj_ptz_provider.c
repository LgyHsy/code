#include "anj_ptz_provider.h"

static const anj_ptz_provider_ops *s_pstPtzProviderOps = 0;

int anj_ptz_provider_register(const anj_ptz_provider_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstPtzProviderOps == 0 ||
        ops->provider_priority >= s_pstPtzProviderOps->provider_priority)
    {
        s_pstPtzProviderOps = ops;
    }

    return 0;
}

void anj_ptz_provider_unregister(const anj_ptz_provider_ops *ops)
{
    if (s_pstPtzProviderOps == ops)
    {
        s_pstPtzProviderOps = 0;
    }
}

int anj_ptz_provider_available(void)
{
    return s_pstPtzProviderOps != 0;
}

int anj_ptz_provider_init(void)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->init)
    {
        return s_pstPtzProviderOps->init();
    }

    return 0;
}

int anj_ptz_provider_uninit(void)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->uninit)
    {
        return s_pstPtzProviderOps->uninit();
    }

    return 0;
}

int anj_ptz_provider_operate(int mode, int arg, int speed)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->operate)
    {
        return s_pstPtzProviderOps->operate(mode, arg, speed);
    }

    return 0;
}

void anj_ptz_provider_debug(void)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->debug)
    {
        s_pstPtzProviderOps->debug();
    }
}

void anj_ptz_provider_dir_set(PtzDir *pstPtzDir)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->dir_set)
    {
        s_pstPtzProviderOps->dir_set(pstPtzDir);
    }
}

void anj_ptz_provider_speed_set(PtzSpeed *pstPtzSpeed)
{
    if (s_pstPtzProviderOps && s_pstPtzProviderOps->speed_set)
    {
        s_pstPtzProviderOps->speed_set(pstPtzSpeed);
    }
}
