#include "anj_mw_comm.h"
#include "anj_mw_smart.h"
#include "anj_smart_provider.h"
#include "anj_sysctl.h"
#include "function_list.h"

static smart_mask_e anj_smart_pd_mask_get(void)
{
    return SMART_SET_MASK(SMART_NULL_MASK, SMART_HUMAN_MASK);
}

static void anj_smart_pd_capability_add(void)
{
    anj_sysctl_capability_add(FUNCTION_ALARM_PD);
    anj_sysctl_capability_add(FUNCTION_PD_RECT_2);
    anj_sysctl_capability_add(FUNCTION_PD_TRACK_HUMAN);
    anj_sysctl_capability_add(FUNCTION_ZOOM_TRACK);
    anj_sysctl_capability_add(FUCTION_PD_POLYGON);
    anj_sysctl_capability_add(FUNCTION_PD_MOSIC);
}

static int anj_smart_pd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    anj_smart_pd_capability_add();
    return anj_mw_smart_init(pstAnjSmartAttr);
}

static int anj_smart_pd_uninit(void)
{
    return anj_mw_smart_uninit();
}

static int anj_smart_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, pstAnjSmartAttr);
}

static const anj_smart_pd_ops s_stSmartPdOps = {
    .provider_name = "pd",
    .provider_priority = 100,
    .mask_get = anj_smart_pd_mask_get,
    .init = anj_smart_pd_init,
    .uninit = anj_smart_pd_uninit,
    .process = anj_smart_pd_process,
};

ANJ_LINK_KEEP(anj_keep_smart_pd_provider);

__attribute__((constructor)) static void anj_smart_pd_provider_register_constructor(void)
{
    anj_smart_pd_provider_register(&s_stSmartPdOps);
}

__attribute__((destructor)) static void anj_smart_pd_provider_unregister_constructor(void)
{
    anj_smart_pd_provider_unregister(&s_stSmartPdOps);
}
