#include <stdint.h>
#include "anj_mw_mutex.h"
#include "anj_mw_time.h"
#include "anj_mw_comm.h"
// static pthread_mutex_t s_Mutex = PTHREAD_MUTEX_INITIALIZER;

int anj_mutex_create(pthread_mutex_t *pMutex, int reentrant)
{
    int iRet = -1;
    if (NULL == pMutex)
    {
        __ERR("input null err\n");
        return iRet;
    }

    if (reentrant)
    {
        pthread_mutexattr_t mutexAttributes;
        if (0 != pthread_mutexattr_init(&mutexAttributes) ||
            0 != pthread_mutexattr_settype(&mutexAttributes, reentrant ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL))
        {
            pthread_mutexattr_destroy(&mutexAttributes);
            iRet = pthread_mutex_init(pMutex, &mutexAttributes);
        }
        else
        {
            __ERR("pthread_mutex_init err \n");
            // iRet = pthread_mutex_init(pMutex, NULL);
        }
    }
    else
    {
        iRet = pthread_mutex_init(pMutex, NULL);
    }

    if (0 != iRet)
    {
        __ERR("pthread_mutex_init err %d\n", iRet);
    }

    return iRet;
}

void anj_mutex_destroy(pthread_mutex_t *pMutex)
{
    if (NULL == pMutex)
    {
        __ERR("input null err\n");
        return;
    }
    __DBG("pMutex %p\n", pMutex);
    pthread_mutex_destroy(pMutex);
    return;
}

void anj_mutex_lock(pthread_mutex_t *pMutex)
{
    __DBG("pMutex %p\n", pMutex);
    pthread_mutex_lock(pMutex);
    return;
}

void anj_mutex_unlock(pthread_mutex_t *pMutex)
{
    __DBG("pMutex %p\n", pMutex);
    pthread_mutex_unlock(pMutex);
    return;
}

int anj_mutex_trylock(pthread_mutex_t *pMutex)
{
    __DBG("pMutex %p\n", pMutex);
    return pthread_mutex_trylock(pMutex);
}

int anj_condition_create(pthread_cond_t *pCondVar)
{
    pthread_condattr_t attr;
    int iRet = -1;
    if (NULL == pCondVar)
    {
        __ERR("input null err\n");
        return iRet;
    }

    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    iRet = pthread_cond_init(pCondVar, &attr);

    if (0 != iRet)
    {
        __ERR("cond %p, ret %d\n", pCondVar, iRet);
        return iRet;
    }
    else
    {
        __INFO("cond %p ok\n", pCondVar);
    }
    return iRet;
}

void anj_condition_destroy(pthread_cond_t *pCondVar)
{
    if (NULL == pCondVar)
    {
        __ERR("input null err\n");
        return;
    }
    __INFO("cond %p ok\n", pCondVar);
    pthread_cond_destroy(pCondVar);
    return;
}

int anj_condition_signal(pthread_cond_t *pCondVar, int bBroadCast)
{
    if (NULL == pCondVar)
    {
        __ERR("input null err\n");
        return -1;
    }
    __DBG("cond %p bBroadCast %d\n", pCondVar, bBroadCast);
    if (bBroadCast)
    {
        return pthread_cond_broadcast(pCondVar);
    }
    else
    {
        return pthread_cond_signal(pCondVar);
    }
}

int anj_condition_wait(pthread_cond_t *pCondVar, pthread_mutex_t *pMutex, unsigned int iTimeoutMs)
{
    int iRet = -1;

    if (NULL == pCondVar || NULL == pMutex)
    {
        __ERR("input null err\n");
        return -1;
    }

    __DBG("cond %p mutex %p timeout %d\n", pCondVar, pMutex, iTimeoutMs);
    if (0 == iTimeoutMs)
    {
        iRet = pthread_cond_wait(pCondVar, pMutex);
    }
    else
    {
        struct timespec timeSpec;
        uint64_t iNowTimeMs = anj_mw_get_cputime_ms(NULL);
        iNowTimeMs += iTimeoutMs;
        anj_mw_msecond_to_timespec(&timeSpec, iNowTimeMs);
        iRet = pthread_cond_timedwait(pCondVar, pMutex, &timeSpec);
    }
    return iRet;
}
