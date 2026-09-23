#ifndef __ANJ_MW_MEM_H__
#define __ANJ_MW_MEM_H__

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* __cplusplus */

void *anj_mw_malloc(size_t size);

void anj_mw_free(void *ptr);

void *anj_mw_calloc(size_t nmemb, size_t size);

void *anj_mw_realloc(void *ptr, size_t size);

void *anj_mw_memalign(size_t alignment, size_t size);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */

#endif
