#include <malloc.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_mem.h"

#define ANJ_MW_DEBUG_MEM (1)

void *anj_mw_malloc(size_t size)
{
#ifdef ANJ_MW_DEBUG_MEM
    if (size > (1024 * 1024))
    {
        __INFO("malloc over buf size %u\n", size);
    }
    void *pBuf = malloc(size);
    if (NULL == pBuf)
    {
        __ERR("malloc null,size %u\n", size);
    }
    return pBuf;
#else
    return malloc(size);
#endif
}

void anj_mw_free(void *ptr)
{
    if (ptr != NULL)
    {
        free(ptr);
    }
}

void *anj_mw_calloc(size_t nmemb, size_t size)
{
#ifdef ANJ_MW_DEBUG_MEM
    void *pBuf = calloc(nmemb, size);
    if (NULL == pBuf)
    {
        __ERR("calloc null,size %u,%u\n", nmemb, size);
    }
    return pBuf;
#else
    return calloc(nmemb, size);
#endif
}

void *anj_mw_realloc(void *ptr, size_t size)
{
#ifdef ANJ_MW_DEBUG_MEM
    void *pBuf = realloc(ptr, size);
    if (NULL == pBuf)
    {
        __ERR("realloc %p,%u\n", ptr, size);
    }
    return pBuf;
#else
    return realloc(ptr, size);
#endif
}

void *anj_mw_memalign(size_t alignment, size_t size)
{
#ifdef ANJ_MW_DEBUG_MEM
    void *pBuf = memalign(size, alignment);
    if (NULL == pBuf)
    {
        __ERR("memalign %u,%u\n", size, alignment);
    }
    return pBuf;
#else
    return memalign(size, alignment);
#endif
}
