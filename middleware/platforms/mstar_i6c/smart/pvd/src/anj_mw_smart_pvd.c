#include "anj_mw_comm.h"
#include "anj_mw_smart_pvd.h"

int anj_mw_smart_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                             AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr)
{
    (void)p_vir_addr;
    (void)p_phy_addr;
    (void)len;
    (void)pstAnjSmartInfo;
    (void)pstAnjPdAttr;
    return 0;
}

int anj_mw_smart_pvd_init(float threshold, int enable)
{
    (void)threshold;
    (void)enable;
    return 0;
}

int anj_mw_smart_pvd_uninit(void)
{
    return 0;
}

ANJ_LINK_KEEP(anj_keep_mw_smart_pvd_provider);
