#ifndef _SK_DEF_H
#define _SK_DEF_H

#include <pthread.h>

#define SK_INVALID_ID (-1)

#define SK_ERROR(info) printf info
#define SK_INFO(info) printf info

typedef pthread_t sk_task_id_t;
typedef pthread_mutex_t sk_mutex_id_t;

typedef enum net_callback_status_t
{
	NET_CB_SUCCESS = 0
}net_callback_status_t;

typedef enum sk_status_code_t
{
	SK_SUCCESS = 0,
	SK_FAILED = -1,
	SK_ERROR = -2,
	SK_ERROR_BAD_PARAMETER = -3,
	SK_ERROR_DEVICE_BUSY = -4,
}sk_status_code_t;


void *sk_mem_malloc(unsigned int mallocsize);
void sk_mem_free(void *mem);

void sk_mutex_create(sk_mutex_id_t *mtx);
void sk_mutex_lock(sk_mutex_id_t *mtx);
void sk_mutex_unlock(sk_mutex_id_t *mtx);

void sk_task_delay(int ms);


#endif
