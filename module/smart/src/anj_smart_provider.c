#include "anj_smart_provider.h"

static const anj_smart_pd_ops *s_pstSmartPdOps = 0;
static const anj_smart_fd_ops *s_pstSmartFdOps = 0;
static const anj_smart_pvd_ops *s_pstSmartPvdOps = 0;
static const anj_smart_md_ops *s_pstSmartMdOps = 0;

int anj_smart_pd_provider_register(const anj_smart_pd_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstSmartPdOps == 0 ||
        ops->provider_priority >= s_pstSmartPdOps->provider_priority)
    {
        s_pstSmartPdOps = ops;
    }

    return 0;
}

void anj_smart_pd_provider_unregister(const anj_smart_pd_ops *ops)
{
    if (s_pstSmartPdOps == ops)
    {
        s_pstSmartPdOps = 0;
    }
}

int anj_smart_fd_provider_register(const anj_smart_fd_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstSmartFdOps == 0 ||
        ops->provider_priority >= s_pstSmartFdOps->provider_priority)
    {
        s_pstSmartFdOps = ops;
    }

    return 0;
}

void anj_smart_fd_provider_unregister(const anj_smart_fd_ops *ops)
{
    if (s_pstSmartFdOps == ops)
    {
        s_pstSmartFdOps = 0;
    }
}

int anj_smart_pvd_provider_register(const anj_smart_pvd_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstSmartPvdOps == 0 ||
        ops->provider_priority >= s_pstSmartPvdOps->provider_priority)
    {
        s_pstSmartPvdOps = ops;
    }

    return 0;
}

void anj_smart_pvd_provider_unregister(const anj_smart_pvd_ops *ops)
{
    if (s_pstSmartPvdOps == ops)
    {
        s_pstSmartPvdOps = 0;
    }
}

int anj_smart_md_provider_register(const anj_smart_md_ops *ops)
{
    if (ops == 0)
    {
        return -1;
    }

    if (s_pstSmartMdOps == 0 ||
        ops->provider_priority >= s_pstSmartMdOps->provider_priority)
    {
        s_pstSmartMdOps = ops;
    }

    return 0;
}

void anj_smart_md_provider_unregister(const anj_smart_md_ops *ops)
{
    if (s_pstSmartMdOps == ops)
    {
        s_pstSmartMdOps = 0;
    }
}

smart_mask_e anj_smart_provider_pd_mask_get(void)
{
    if (s_pstSmartPdOps && s_pstSmartPdOps->mask_get)
    {
        return s_pstSmartPdOps->mask_get();
    }

    return SMART_NULL_MASK;
}

smart_mask_e anj_smart_provider_fd_mask_get(void)
{
    if (s_pstSmartFdOps && s_pstSmartFdOps->mask_get)
    {
        return s_pstSmartFdOps->mask_get();
    }

    return SMART_NULL_MASK;
}

smart_mask_e anj_smart_provider_pvd_mask_get(void)
{
    if (s_pstSmartPvdOps && s_pstSmartPvdOps->mask_get)
    {
        return s_pstSmartPvdOps->mask_get();
    }

    return SMART_NULL_MASK;
}

smart_mask_e anj_smart_provider_md_mask_get(void)
{
    if (s_pstSmartMdOps && s_pstSmartMdOps->mask_get)
    {
        return s_pstSmartMdOps->mask_get();
    }

    return SMART_NULL_MASK;
}

int anj_smart_provider_pd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    if (s_pstSmartPdOps && s_pstSmartPdOps->init)
    {
        return s_pstSmartPdOps->init(pstAnjSmartAttr, pfnYuvCb);
    }

    return 0;
}

int anj_smart_provider_pd_uninit(void)
{
    if (s_pstSmartPdOps && s_pstSmartPdOps->uninit)
    {
        return s_pstSmartPdOps->uninit();
    }

    return 0;
}

int anj_smart_provider_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                  AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    if (s_pstSmartPdOps && s_pstSmartPdOps->process)
    {
        return s_pstSmartPdOps->process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
    }

    return 0;
}

int anj_smart_provider_fd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    if (s_pstSmartFdOps && s_pstSmartFdOps->init)
    {
        return s_pstSmartFdOps->init(pstAnjSmartAttr, pfnYuvCb);
    }

    return 0;
}

int anj_smart_provider_fd_uninit(void)
{
    if (s_pstSmartFdOps && s_pstSmartFdOps->uninit)
    {
        return s_pstSmartFdOps->uninit();
    }

    return 0;
}

int anj_smart_provider_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                  AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    if (s_pstSmartFdOps && s_pstSmartFdOps->process)
    {
        return s_pstSmartFdOps->process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
    }

    return 0;
}

int anj_smart_provider_pvd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    if (s_pstSmartPvdOps && s_pstSmartPvdOps->init)
    {
        return s_pstSmartPvdOps->init(pstAnjSmartAttr, pfnYuvCb);
    }

    return 0;
}

int anj_smart_provider_pvd_uninit(void)
{
    if (s_pstSmartPvdOps && s_pstSmartPvdOps->uninit)
    {
        return s_pstSmartPvdOps->uninit();
    }

    return 0;
}

int anj_smart_provider_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                   AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    if (s_pstSmartPvdOps && s_pstSmartPvdOps->process)
    {
        return s_pstSmartPvdOps->process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
    }

    return 0;
}

int anj_smart_provider_md_init(int pic_width, int pic_height)
{
    if (s_pstSmartMdOps && s_pstSmartMdOps->init)
    {
        return s_pstSmartMdOps->init(pic_width, pic_height);
    }

    return 0;
}

int anj_smart_provider_md_uninit(void)
{
    if (s_pstSmartMdOps && s_pstSmartMdOps->uninit)
    {
        return s_pstSmartMdOps->uninit();
    }

    return 0;
}

int anj_smart_provider_md_process(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult)
{
    if (s_pstSmartMdOps && s_pstSmartMdOps->process)
    {
        return s_pstSmartMdOps->process(p_vir_addr, len, cameraIndex, pstMdResult);
    }

    return 0;
}
