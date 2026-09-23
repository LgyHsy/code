#include "anj_mw_ring_queue.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#include "anj_mw_log.h"
#include "anj_mw_mutex.h"

static void anj_mw_rq_put_mutex_enter(ANJ_MW_RING_QUEUE *queue)
{
    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        anj_mutex_lock((pthread_mutex_t *)queue->queue_put_mutex);
    }
}

static void anj_mw_rq_put_mutex_leave(ANJ_MW_RING_QUEUE *queue)
{
    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        anj_mutex_unlock((pthread_mutex_t *)queue->queue_put_mutex);
    }
}

ANJ_MW_RING_QUEUE *anj_mw_rq_create(uint32_t unit_num, uint32_t unit_size, uint32_t queue_mode)
{
    uint32_t q_len = unit_num * unit_size + sizeof(ANJ_MW_RING_QUEUE);
    ANJ_MW_RING_QUEUE *queue = (ANJ_MW_RING_QUEUE *)malloc(q_len);
    if (queue == NULL)
    {
        __ERR("%s, malloc ANJ_MW_RING_QUEUE fail\r\n", __func__);
        return NULL;
    }

    queue->queue_mode = queue_mode;
    queue->unit_size = unit_size;
    queue->unit_num = unit_num;
    queue->front = 0;
    queue->rear = 0;
    queue->count_put_full = 0;
    queue->queue_buffer = sizeof(ANJ_MW_RING_QUEUE);

    if (queue_mode & ANJ_MW_RQ_NO_EVENT)
    {
        queue->queue_not_null_event = NULL;
        queue->queue_not_full_event = NULL;
        queue->queue_put_mutex = NULL;
    }
    else
    {
        queue->queue_not_null_event = malloc(sizeof(sem_t));
        queue->queue_not_full_event = malloc(sizeof(sem_t));
        queue->queue_put_mutex = malloc(sizeof(pthread_mutex_t));

        if (queue->queue_not_null_event)
        {
            sem_init((sem_t *)queue->queue_not_null_event, 0, 0);
        }
        if (queue->queue_not_full_event)
        {
            sem_init((sem_t *)queue->queue_not_full_event, 0, 0);
        }
        if (queue->queue_put_mutex)
        {
            anj_mutex_create((pthread_mutex_t *)queue->queue_put_mutex, 0);
        }
    }

    return queue;
}

void anj_mw_rq_delete(ANJ_MW_RING_QUEUE *queue)
{
    if (queue == NULL)
    {
        return;
    }

    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        if (queue->queue_not_null_event)
        {
            sem_destroy((sem_t *)queue->queue_not_null_event);
            free(queue->queue_not_null_event);
        }
        if (queue->queue_not_full_event)
        {
            sem_destroy((sem_t *)queue->queue_not_full_event);
            free(queue->queue_not_full_event);
        }
        if (queue->queue_put_mutex)
        {
            anj_mutex_destroy((pthread_mutex_t *)queue->queue_put_mutex);
            free(queue->queue_put_mutex);
        }
    }

    free(queue);
}

int anj_mw_rq_put(ANJ_MW_RING_QUEUE *queue, const char *buf)
{
    uint32_t real_rear, queue_count;
    char *ptr;

    if (queue == NULL || buf == NULL)
    {
        return 0;
    }

    anj_mw_rq_put_mutex_enter(queue);

put_start:
    queue_count = queue->rear - queue->front;
    if (queue_count == (queue->unit_num - 1))
    {
        if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
        {
            if (queue->queue_mode & ANJ_MW_RQ_PUT_WAIT)
            {
                sem_wait((sem_t *)queue->queue_not_full_event);
                goto put_start;
            }

            queue->count_put_full++;
            anj_mw_rq_put_mutex_leave(queue);
            return 0;
        }

        anj_mw_rq_put_mutex_leave(queue);
        return 0;
    }

    real_rear = queue->rear % queue->unit_num;
    ptr = ((char *)queue) + queue->queue_buffer + real_rear * queue->unit_size;
    memcpy(ptr, buf, queue->unit_size);
    queue->rear++;

    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        sem_post((sem_t *)queue->queue_not_null_event);
    }

    anj_mw_rq_put_mutex_leave(queue);
    return 1;
}

int anj_mw_rq_get(ANJ_MW_RING_QUEUE *queue, char *buf)
{
    uint32_t real_front;

    if (queue == NULL || buf == NULL)
    {
        return 0;
    }

get_start:
    if (queue->front == queue->rear)
    {
        if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
        {
            if (queue->queue_mode & ANJ_MW_RQ_GET_WAIT)
            {
                sem_wait((sem_t *)queue->queue_not_null_event);
                goto get_start;
            }
            return 0;
        }
        return 0;
    }

    real_front = queue->front % queue->unit_num;
    memcpy(buf, ((char *)queue) + queue->queue_buffer + real_front * queue->unit_size, queue->unit_size);
    queue->front++;

    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        sem_post((sem_t *)queue->queue_not_full_event);
    }

    return 1;
}

int anj_mw_rq_is_empty(ANJ_MW_RING_QUEUE *queue)
{
    if (queue == NULL)
    {
        return 1;
    }
    return (queue->front == queue->rear) ? 1 : 0;
}

int anj_mw_rq_is_full(ANJ_MW_RING_QUEUE *queue)
{
    uint32_t queue_count;
    if (queue == NULL)
    {
        return 0;
    }
    queue_count = queue->rear - queue->front;
    return (queue_count == (queue->unit_num - 1)) ? 1 : 0;
}

char *anj_mw_rq_get_wait(ANJ_MW_RING_QUEUE *queue)
{
    uint32_t real_front;

    if (queue == NULL)
    {
        return NULL;
    }

get_wait_start:
    if (queue->front == queue->rear)
    {
        if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
        {
            if (queue->queue_mode & ANJ_MW_RQ_GET_WAIT)
            {
                sem_wait((sem_t *)queue->queue_not_null_event);
                goto get_wait_start;
            }
            return NULL;
        }
        return NULL;
    }

    real_front = queue->front % queue->unit_num;
    return ((char *)queue) + queue->queue_buffer + real_front * queue->unit_size;
}

void anj_mw_rq_get_wait_post(ANJ_MW_RING_QUEUE *queue)
{
    if (queue == NULL)
    {
        return;
    }

    queue->front++;
    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        sem_post((sem_t *)queue->queue_not_full_event);
    }
}

char *anj_mw_rq_put_ptr_wait(ANJ_MW_RING_QUEUE *queue)
{
    uint32_t real_rear, queue_count;
    char *ptr;

    if (queue == NULL)
    {
        return NULL;
    }

    anj_mw_rq_put_mutex_enter(queue);

put_ptr_start:
    queue_count = queue->rear - queue->front;
    if (queue_count == (queue->unit_num - 1))
    {
        if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
        {
            if (queue->queue_mode & ANJ_MW_RQ_PUT_WAIT)
            {
                sem_wait((sem_t *)queue->queue_not_full_event);
                goto put_ptr_start;
            }
            queue->count_put_full++;
            anj_mw_rq_put_mutex_leave(queue);
            return NULL;
        }

        anj_mw_rq_put_mutex_leave(queue);
        return NULL;
    }

    real_rear = queue->rear % queue->unit_num;
    ptr = ((char *)queue) + queue->queue_buffer + real_rear * queue->unit_size;
    return ptr;
}

void anj_mw_rq_put_ptr_wait_post(ANJ_MW_RING_QUEUE *queue, int put_finish)
{
    if (queue == NULL)
    {
        return;
    }

    if (put_finish)
    {
        queue->rear++;
    }

    if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
    {
        sem_post((sem_t *)queue->queue_not_null_event);
    }

    anj_mw_rq_put_mutex_leave(queue);
}

int anj_mw_rq_peek(ANJ_MW_RING_QUEUE *queue, char *buf)
{
    uint32_t real_front;

    if (queue == NULL || buf == NULL)
    {
        return 0;
    }

peek_start:
    if (queue->front == queue->rear)
    {
        if ((queue->queue_mode & ANJ_MW_RQ_NO_EVENT) == 0)
        {
            if (queue->queue_mode & ANJ_MW_RQ_GET_WAIT)
            {
                sem_wait((sem_t *)queue->queue_not_null_event);
                goto peek_start;
            }
            return 0;
        }
        return 0;
    }

    real_front = queue->front % queue->unit_num;
    memcpy(buf, ((char *)queue) + queue->queue_buffer + real_front * queue->unit_size, queue->unit_size);
    return 1;
}
