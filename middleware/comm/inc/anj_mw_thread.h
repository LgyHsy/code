#ifndef _ANJ_MW_THREAD_H_
#define _ANJ_MW_THREAD_H_

#include "anj_mw_comm.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int (*func)(void *ctx, int *bStart);
    void *ctx;
} anj_thread_job_s;

typedef struct
{
    pthread_t pid;
    int start;
    int end;
    int bAutoDestroy;
    char iThreadName[32];
    anj_thread_job_s iThreadjob; // 必须设置
} anj_thread_s;

typedef void *(*thread_task)(void *);

int anj_thread_task_create(anj_thread_s *pAnjThread);

int anj_thread_task_destroy(anj_thread_s *pAnjThread, int iWaitTimems);

int anj_thread_create(pthread_t *thread, int priority, const char *name,
                        void *(*thread_task)(void *), void *arg, int bAutoDestroy);

void anj_thread_destroy(pthread_t thread);

#ifdef __cplusplus
}
#endif

#endif
