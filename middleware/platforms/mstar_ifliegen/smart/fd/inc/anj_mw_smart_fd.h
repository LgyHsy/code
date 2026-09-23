#ifndef _ANJ_MW_SMART_FD_H_
#define _ANJ_MW_SMART_FD_H_

#include "anj_mw_smart.h"

#ifdef __cplusplus
extern "C"
{
#endif

int anj_mw_smart_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                            AnjSmartInfo *pstAnjSmartInfo, AnjSmartFdAttr *pstAnjFdAttr);

int anj_mw_smart_fd_init(float threshold, int enable);

int anj_mw_smart_fd_uninit(void);

#ifdef __cplusplus
}
#endif

#endif
