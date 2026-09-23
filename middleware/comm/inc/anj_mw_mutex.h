#ifndef _ANJ_MW_MUTEX_H_
#define _ANJ_MW_MUTEX_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

int anj_mutex_create(pthread_mutex_t *pMutex, int reentrant);
void anj_mutex_destroy(pthread_mutex_t *pMutex);
void anj_mutex_lock(pthread_mutex_t *pMutex);
void anj_mutex_unlock(pthread_mutex_t *pMutex);
int anj_mutex_trylock(pthread_mutex_t *pMutex);
int anj_condition_create(pthread_cond_t *pCondVar);
void anj_condition_destroy(pthread_cond_t *pCondVar);
int anj_condition_signal(pthread_cond_t *pCondVar, int bBroadCast);
int anj_condition_wait(pthread_cond_t *pCondVar, pthread_mutex_t *pMutex, unsigned int iTimeoutMs);

#ifdef __cplusplus
}
#endif

#endif
