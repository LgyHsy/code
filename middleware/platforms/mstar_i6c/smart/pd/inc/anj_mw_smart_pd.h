#ifndef _ANJ_MW_SMART_PD_H_
#define _ANJ_MW_SMART_PD_H_

#include "anj_mw_smart.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

int anj_mw_smart_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjHdBoxInfo, AnjSmartPdAttr *pstAnjPdAttr);

int anj_mw_smart_pd_init(float threshold);

int anj_mw_smart_pd_uninit();

#ifdef __cplusplus
}
#endif

#endif
