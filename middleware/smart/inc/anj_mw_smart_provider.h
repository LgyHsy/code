#ifndef __ANJ_MW_SMART_PROVIDER_H__
#define __ANJ_MW_SMART_PROVIDER_H__

#include "anj_mw_smart.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    const char *provider_name;
    ANJ_SMART_TYPE_E type; /* SMART_TYPE_FD → fd 槽；PD/PVD → body 槽 */
    int provider_priority;
    int (*init)(AnjSmartAttr *pstAnjSmartAttr);
    int (*uninit)(void);
    float (*set_sensitivity)(float sensitivity);
    int (*process)(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
    int (*size_get)(unsigned int *width, unsigned int *height);
} anj_mw_smart_provider_ops;

int anj_mw_smart_provider_register(const anj_mw_smart_provider_ops *ops);
void anj_mw_smart_provider_unregister(const anj_mw_smart_provider_ops *ops);
int anj_mw_smart_provider_init(AnjSmartAttr *pstAnjSmartAttr);
int anj_mw_smart_provider_uninit(void);
float anj_mw_smart_provider_set_sensitivity(float sensitivity);
int anj_mw_smart_provider_process(void *p_vir_addr, unsigned long long p_phy_addr, int len, AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
/* pstAnjSmartAttr 决定 fd/body；fd.enable 优先。无 attr 或未开对应算法返回 -1 */
int anj_mw_smart_provider_size_get(AnjSmartAttr *pstAnjSmartAttr, unsigned int *width, unsigned int *height);

#ifdef __cplusplus
}
#endif

#endif
