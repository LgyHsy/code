#include "anj_mw_smart_provider.h"

static const anj_mw_smart_provider_ops *s_pstBodyOps = 0;
static const anj_mw_smart_provider_ops *s_pstFdOps = 0;

int anj_mw_smart_provider_register(const anj_mw_smart_provider_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (ops->type == SMART_TYPE_FD)
    {
        if (s_pstFdOps == 0 || ops->provider_priority >= s_pstFdOps->provider_priority)
        {
            s_pstFdOps = ops;
        }
    }
    else
    {
        /* PD / PVD 等 body 算法，编译期互斥 */
        if (s_pstBodyOps == 0 || ops->provider_priority >= s_pstBodyOps->provider_priority)
        {
            s_pstBodyOps = ops;
        }
    }

    return 0;
}

void anj_mw_smart_provider_unregister(const anj_mw_smart_provider_ops *ops)
{
    if (s_pstBodyOps == ops)
    {
        s_pstBodyOps = 0;
    }

    if (s_pstFdOps == ops)
    {
        s_pstFdOps = 0;
    }
}

int anj_mw_smart_provider_init(AnjSmartAttr *pstAnjSmartAttr)
{
    if (s_pstBodyOps && s_pstBodyOps->init)
    {
        return s_pstBodyOps->init(pstAnjSmartAttr);
    }

    return 0;
}

int anj_mw_smart_provider_uninit(void)
{
    if (s_pstBodyOps && s_pstBodyOps->uninit)
    {
        return s_pstBodyOps->uninit();
    }

    return 0;
}

float anj_mw_smart_provider_set_sensitivity(float sensitivity)
{
    if (s_pstBodyOps && s_pstBodyOps->set_sensitivity)
    {
        return s_pstBodyOps->set_sensitivity(sensitivity);
    }

    return 0;
}

int anj_mw_smart_provider_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    if (s_pstBodyOps && s_pstBodyOps->process)
    {
        return s_pstBodyOps->process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
    }

    return 0;
}

int anj_mw_smart_provider_size_get(AnjSmartAttr *pstAnjSmartAttr, unsigned int *width, unsigned int *height)
{
    int fd_en = 0;
    int body_en = 0;

    if ((pstAnjSmartAttr == NULL) || (width == NULL) || (height == NULL))
    {
        return -1;
    }

    fd_en = pstAnjSmartAttr->stAnjFdAttr.enable;
    body_en = pstAnjSmartAttr->stAnjPdAttr.enable;

    if (fd_en && s_pstFdOps && s_pstFdOps->size_get)
    {
        return s_pstFdOps->size_get(width, height);
    }

    if (body_en && s_pstBodyOps && s_pstBodyOps->size_get)
    {
        return s_pstBodyOps->size_get(width, height);
    }

    return -1;
}
