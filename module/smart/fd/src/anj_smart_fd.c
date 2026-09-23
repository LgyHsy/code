#include "anj_mw_comm.h"
#include "anj_mw_smart_fd.h"
#include "anj_smart_provider.h"
#include "anj_sysctl.h"
#include "function_list.h"

static smart_mask_e anj_smart_fd_mask_get(void)
{
    return SMART_SET_MASK(SMART_NULL_MASK, SMART_FACE_MASK);
}

static int anj_smart_fd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb)
{
    int iRet = 0;

	anj_sysctl_capability_add(FUNCTION_FACE_FD);
    if (pstAnjSmartAttr == NULL)
    {
        return -1;
    }

    iRet = anj_mw_smart_fd_init(pstAnjSmartAttr->stAnjFdAttr.sensitivity,
                                pstAnjSmartAttr->stAnjFdAttr.enable);
    if (iRet != 0)
    {
        anj_mw_smart_fd_uninit();
        return iRet;
    }

    return 0;
}

static int anj_smart_fd_uninit(void)
{
    return anj_mw_smart_fd_uninit();
}

static int anj_smart_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr)
{
    return anj_mw_smart_fd_process(p_vir_addr, p_phy_addr, len, pstAnjSmartInfo, &pstAnjSmartAttr->stAnjFdAttr);
}

static const anj_smart_fd_ops s_stSmartFdOps = {
    .provider_name = "fd",
    .provider_priority = 100,
    .mask_get = anj_smart_fd_mask_get,
    .init = anj_smart_fd_init,
    .uninit = anj_smart_fd_uninit,
    .process = anj_smart_fd_process,
};

ANJ_LINK_KEEP(anj_keep_smart_fd_provider);

__attribute__((constructor)) static void anj_smart_fd_provider_register_constructor(void)
{
    anj_smart_fd_provider_register(&s_stSmartFdOps);
}

__attribute__((destructor)) static void anj_smart_fd_provider_unregister_constructor(void)
{
    anj_smart_fd_provider_unregister(&s_stSmartFdOps);
}
