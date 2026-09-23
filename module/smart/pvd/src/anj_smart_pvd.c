#include "anj_mw_comm.h"
#include "anj_mw_smart_pvd.h"
#include "anj_smart_provider.h"
#include "anj_sysctl.h"
#include "function_list.h"

static smart_mask_e anj_smart_pvd_mask_get(void)
{
    return SMART_SET_MASK(SMART_SET_MASK(SMART_NULL_MASK, SMART_HUMAN_MASK), SMART_CAR_MASK);
}

static void anj_smart_pvd_capability_add(void)
{
    anj_sysctl_capability_add(FUNCTION_ALARM_PD);
    anj_sysctl_capability_add(FUNCTION_PD_RECT_2);
    anj_sysctl_capability_add(FUNCTION_PD_TRACK_HUMAN);
    anj_sysctl_capability_add(FUNCTION_ZOOM_TRACK);
    anj_sysctl_capability_add(FUCTION_PD_POLYGON);
    anj_sysctl_capability_add(FUNCTION_PD_MOSIC);
    anj_sysctl_capability_add(FUNCTION_ALARM_VEHICLE_CAR);
    anj_sysctl_capability_add(FUNCTION_ALARM_VEHICLE_BICYCLE);
    anj_sysctl_capability_add(FUNCTION_ALARM_VEHICLE_MOTO);
}

static int anj_smart_pvd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    int iRet = 0;
    anj_smart_pvd_capability_add();

    if (pstAnjSmartAttr == NULL)
    {
        return -1;
    }

    iRet = anj_mw_smart_pvd_init(pstAnjSmartAttr->stAnjPdAttr.sensitivity,
                                 pstAnjSmartAttr->stAnjPdAttr.enable);
    if (iRet != 0)
    {
        anj_mw_smart_pvd_uninit();
        return iRet;
    }

    return 0;
}

static int anj_smart_pvd_uninit(void)
{
    return anj_mw_smart_pvd_uninit();
}

static int anj_smart_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                 AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_pvd_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, &pstAnjSmartAttr->stAnjPdAttr);
}

static const anj_smart_pvd_ops s_stSmartPvdOps = {
    .provider_name = "pvd",
    .provider_priority = 100,
    .mask_get = anj_smart_pvd_mask_get,
    .init = anj_smart_pvd_init,
    .uninit = anj_smart_pvd_uninit,
    .process = anj_smart_pvd_process,
};

ANJ_LINK_KEEP(anj_keep_smart_pvd_provider);

__attribute__((constructor)) static void anj_smart_pvd_provider_register_constructor(void)
{
    anj_smart_pvd_provider_register(&s_stSmartPvdOps);
}

__attribute__((destructor)) static void anj_smart_pvd_provider_unregister_constructor(void)
{
    anj_smart_pvd_provider_unregister(&s_stSmartPvdOps);
}
