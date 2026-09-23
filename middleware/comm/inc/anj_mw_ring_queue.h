#ifndef _ANJ_MW_RING_QUEUE_H_
#define _ANJ_MW_RING_QUEUE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANJ_MW_RQ_PUT_WAIT 0x00000001
#define ANJ_MW_RQ_GET_WAIT 0x00000002
#define ANJ_MW_RQ_NO_EVENT 0x00000004

typedef struct anj_mw_ring_queue
{
    uint32_t queue_mode;
    uint32_t unit_num;
    uint32_t unit_size;
    uint32_t front;
    uint32_t rear;
    uint32_t queue_buffer;
    uint32_t count_put_full;

    void *queue_put_mutex;
    void *queue_not_null_event;
    void *queue_not_full_event;
} ANJ_MW_RING_QUEUE;

ANJ_MW_RING_QUEUE *anj_mw_rq_create(uint32_t unit_num, uint32_t unit_size, uint32_t queue_mode);
void anj_mw_rq_delete(ANJ_MW_RING_QUEUE *queue);

int anj_mw_rq_put(ANJ_MW_RING_QUEUE *queue, const char *buf);
int anj_mw_rq_get(ANJ_MW_RING_QUEUE *queue, char *buf);
int anj_mw_rq_peek(ANJ_MW_RING_QUEUE *queue, char *buf);

int anj_mw_rq_is_empty(ANJ_MW_RING_QUEUE *queue);
int anj_mw_rq_is_full(ANJ_MW_RING_QUEUE *queue);

char *anj_mw_rq_get_wait(ANJ_MW_RING_QUEUE *queue);
void anj_mw_rq_get_wait_post(ANJ_MW_RING_QUEUE *queue);

char *anj_mw_rq_put_ptr_wait(ANJ_MW_RING_QUEUE *queue);
void anj_mw_rq_put_ptr_wait_post(ANJ_MW_RING_QUEUE *queue, int put_finish);

#ifdef __cplusplus
}
#endif

#endif
