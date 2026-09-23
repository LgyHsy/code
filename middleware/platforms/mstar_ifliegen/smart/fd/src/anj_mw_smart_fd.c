#include "anj_mw_comm.h"
#include "anj_mw_smart_fd.h"

int anj_mw_smart_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                            AnjSmartInfo *pstAnjSmartInfo, AnjSmartFdAttr *pstAnjFdAttr)
{
    (void)p_vir_addr;
    (void)p_phy_addr;
    (void)len;
    (void)pstAnjSmartInfo;
    (void)pstAnjFdAttr;
    return 0;
}

int anj_mw_smart_fd_init(float threshold, int enable)
{
    (void)threshold;
    (void)enable;
    return 0;
}

int anj_mw_smart_fd_uninit(void)
{
    return 0;
}

ANJ_LINK_KEEP(anj_keep_mw_smart_fd_provider);
