#ifndef _ANJ_MW_SMART_PVD_H_
#define _ANJ_MW_SMART_PVD_H_

#include "anj_mw_smart.h"

#ifdef __cplusplus
extern "C"
{
#endif

int anj_mw_smart_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                             AnjSmartInfo *pstAnjSmartInfo, AnjSmartPdAttr *pstAnjPdAttr);

int anj_mw_smart_pvd_init(float threshold, int enable);

int anj_mw_smart_pvd_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
