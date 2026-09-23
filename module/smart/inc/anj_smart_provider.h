#ifndef __ANJ_SMART_PROVIDER_H__
#define __ANJ_SMART_PROVIDER_H__

#include <pthread.h>

#include "anj_config.h"
#include "anj_mw_smart.h"
#include "anj_smart.h"
#include "anj_smart_md.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *provider_name;
    int provider_priority;
    smart_mask_e (*mask_get)(void);
    int (*init)(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
    int (*uninit)(void);
    int (*process)(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                   AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
} anj_smart_pd_ops;

typedef struct
{
    const char *provider_name;
    int provider_priority;
    smart_mask_e (*mask_get)(void);
    int (*init)(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
    int (*uninit)(void);
    int (*process)(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                   AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
} anj_smart_fd_ops;

typedef struct
{
    const char *provider_name;
    int provider_priority;
    smart_mask_e (*mask_get)(void);
    int (*init)(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
    int (*uninit)(void);
    int (*process)(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                   AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
} anj_smart_pvd_ops;

typedef struct
{
    const char *provider_name;
    int provider_priority;
    smart_mask_e (*mask_get)(void);
    int (*init)(int pic_width, int pic_height);
    int (*uninit)(void);
    int (*process)(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult);
} anj_smart_md_ops;

int anj_smart_pd_provider_register(const anj_smart_pd_ops *ops);
void anj_smart_pd_provider_unregister(const anj_smart_pd_ops *ops);
int anj_smart_fd_provider_register(const anj_smart_fd_ops *ops);
void anj_smart_fd_provider_unregister(const anj_smart_fd_ops *ops);
int anj_smart_pvd_provider_register(const anj_smart_pvd_ops *ops);
void anj_smart_pvd_provider_unregister(const anj_smart_pvd_ops *ops);
int anj_smart_md_provider_register(const anj_smart_md_ops *ops);
void anj_smart_md_provider_unregister(const anj_smart_md_ops *ops);

smart_mask_e anj_smart_provider_pd_mask_get(void);
smart_mask_e anj_smart_provider_fd_mask_get(void);
smart_mask_e anj_smart_provider_pvd_mask_get(void);
smart_mask_e anj_smart_provider_md_mask_get(void);
int anj_smart_provider_pd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
int anj_smart_provider_pd_uninit(void);
int anj_smart_provider_pd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                  AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
int anj_smart_provider_fd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
int anj_smart_provider_fd_uninit(void);
int anj_smart_provider_fd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                  AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
int anj_smart_provider_pvd_init(AnjSmartAttr *pstAnjSmartAttr, void *pfnYuvCb);
int anj_smart_provider_pvd_uninit(void);
int anj_smart_provider_pvd_process(void *p_vir_addr, unsigned long long p_phy_addr, int len,
                                   AnjSmartInfo *pstAnjSmartInfo, AnjSmartAttr *pstAnjSmartAttr);
int anj_smart_provider_md_init(int pic_width, int pic_height);
int anj_smart_provider_md_uninit(void);
int anj_smart_provider_md_process(void *p_vir_addr, int len, int cameraIndex, MD_RESULT_S *pstMdResult);

#ifdef __cplusplus
}
#endif

#endif
