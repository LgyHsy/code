#include "anj_mw_thread.h"

#define ANJ_THREAD_WAIT_MAX_TIMEMS (100) // 等待每次100ms

static unsigned int s_mThreadNums;
static unsigned int s_mThreadCreateTimes;

static void *anj_thread_task_proc(void *arg)
{
    int iRet = 0;
    anj_thread_s *pAnjThread = (anj_thread_s *)arg;
    if (pAnjThread && pAnjThread->iThreadjob.func)
    {
        __DBG("Create thread(%p:%s,%ld) start\n", pAnjThread, pAnjThread->iThreadName, pAnjThread->pid);
        pAnjThread->end = 0;
        iRet = pAnjThread->iThreadjob.func(pAnjThread->iThreadjob.ctx, &pAnjThread->start);
        __DBG("End thread(%p:%s,%ld) iRet %d, start(%d)\n", pAnjThread, pAnjThread->iThreadName, pAnjThread->pid, iRet, pAnjThread->start);
        pAnjThread->end = 1;
    }
    else
    {
        __ERR("Invalid thread input %p\n", pAnjThread);
    }
    return NULL;
}

int anj_thread_task_create(anj_thread_s *pAnjThread)
{
    int iRet = -1;
    if (pAnjThread && pAnjThread->iThreadjob.func)
    {
        if (pAnjThread->start)
        {
            __ERR("had been start thread\n");
        }
        else
        {
            pAnjThread->start = 1;
            iRet = anj_thread_create(&pAnjThread->pid, 0, pAnjThread->iThreadName, anj_thread_task_proc, pAnjThread, pAnjThread->bAutoDestroy);
            if (0 != iRet)
            {
                pAnjThread->start = 0;
                __ERR("Create thread(%p:%s) Err %d\n", pAnjThread, pAnjThread->iThreadName, iRet);
            }
        }
    }
    else
    {
        __ERR("Invalid thread input %p\n", pAnjThread);
    }

    return iRet;
}

int anj_thread_task_destroy(anj_thread_s *pAnjThread, int iWaitTimems)
{
    int iRet = -1;
    if (pAnjThread)
    {
        if (!pAnjThread->start)
        {
            __ERR("not start thread\n");
        }
        else
        {
            __INFO("thread(%s)\n", pAnjThread->iThreadName);
            pAnjThread->start = 0;
            // 非自动销毁需要阻塞销毁
            if ((0 == pAnjThread->bAutoDestroy) || (0 == iWaitTimems))
            {
                anj_thread_destroy(pAnjThread->pid);
            }
            else
            {
                while (0 == pAnjThread->end)
                {
                    if (iWaitTimems > 0)
                    {
                        __DBG("Wait thread(%p:%s,%ld) End Timems(%d)\n", pAnjThread, pAnjThread->iThreadName, pAnjThread->pid, iWaitTimems);
                        if (iWaitTimems > ANJ_THREAD_WAIT_MAX_TIMEMS)
                        {

                            usleep(ANJ_THREAD_WAIT_MAX_TIMEMS * 1000);
                            iWaitTimems -= ANJ_THREAD_WAIT_MAX_TIMEMS;
                        }
                        else
                        {
                            usleep(iWaitTimems * 1000);
                            break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if (0 == pAnjThread->end)
            {
                __DBG("thread(%p:%s,%ld) Not End\n", pAnjThread, pAnjThread->iThreadName, pAnjThread->pid);
            }
            else
            {
            }
            iRet = pAnjThread->end;
        }
    }
    else
    {
        __ERR("Invalid thread input %p\n", pAnjThread);
    }

    return iRet;
}

int anj_thread_create(pthread_t *thread, int priority, const char *name,
                      void *(*thread_task)(void *), void *arg, int bAutoDestroy)
{
    int iRet = -1;

    iRet = pthread_create(thread, NULL, thread_task, arg);
    if (0 == iRet)
    {
        if (name)
        {
            if (strlen(name) > 0)
            {
                pthread_setname_np(*thread, name);
            }

            __DBG("Create thread(%ld:%s), destroy(%d)(num:%u,%u) Ok\n", *thread, name, bAutoDestroy,
                  s_mThreadCreateTimes, s_mThreadNums);
        }
        else
        {
            __DBG("Create thread(%ld), destroy(%d)(num:%u,%u) Ok\n", *thread, bAutoDestroy,
                  s_mThreadCreateTimes, s_mThreadNums);
        }

        if (bAutoDestroy)
        {
            pthread_detach(*thread);
        }
        s_mThreadNums++;
        s_mThreadCreateTimes++;
    }
    else
    {
        if (name)
        {
            __ERR("Create thread(%ld:%s), destroy(%d)(num:%u,%u) Err\n", *thread, name, bAutoDestroy,
                  s_mThreadCreateTimes, s_mThreadNums);
        }
        else
        {
            __ERR("Create thread(%ld), destroy(%d)(num:%u,%u) Err\n", *thread, bAutoDestroy,
                  s_mThreadCreateTimes, s_mThreadNums);
        }
    }

    return iRet;
}

void anj_thread_destroy(pthread_t thread)
{
    // 如果cancel会直接销毁
    // __INFO("cancel thread(%d)(num:%u,%u) now\n", thread, s_mThreadCreateTimes, s_mThreadNums);
    // pthread_cancel(thread);
    __DBG("join thread(%ld)(num:%u,%u) now\n", thread, s_mThreadCreateTimes, s_mThreadNums);
    pthread_join(thread, NULL);
    s_mThreadNums--;
    __DBG("destroy thread(%ld)(num:%u,%u) End\n", thread, s_mThreadCreateTimes, s_mThreadNums);
}
