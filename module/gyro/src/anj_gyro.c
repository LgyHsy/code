#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_mutex.h"
#include "anj_mw_thread.h"
#include "anj_module.h"
#include "eventhub.h"

#include "anj_gyro.h"
#include "anj_gyro_provider.h"

static int s_gyro_ready = 0;
static anj_gyro_data_t s_gyro_data;
static pthread_mutex_t s_gyro_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_gyro_thread = {0};

static void anj_gyro_data_event_get(EventResult *event_result, void *data)
{
    if (event_result == NULL || data == NULL || !s_gyro_ready)
    {
        return;
    }

    anj_mutex_lock(&s_gyro_data_mutex);
    event_gyro_data_t *event_gyro_data = (event_gyro_data_t *)data;
    event_gyro_data->gyro_x  = s_gyro_data.gyro_x;
    event_gyro_data->gyro_y  = s_gyro_data.gyro_y;
    event_gyro_data->gyro_z  = s_gyro_data.gyro_z;
    event_gyro_data->accel_x = s_gyro_data.accel_x;
    event_gyro_data->accel_y = s_gyro_data.accel_y;
    event_gyro_data->accel_z = s_gyro_data.accel_z;
    event_gyro_data->temp    = s_gyro_data.temp;
    anj_mutex_unlock(&s_gyro_data_mutex);

    event_result->ret = 0;
}

static void anj_gyro_data_cache_update(const anj_gyro_data_t *data)
{
    anj_mutex_lock(&s_gyro_data_mutex);
    s_gyro_data = *data;
    s_gyro_data.ready = 1;
    anj_mutex_unlock(&s_gyro_data_mutex);
}

static int anj_gyro_sample_thread(void *ctx, int *bStart)
{
    anj_gyro_data_t data;
    int sample_interval_ms = anj_gyro_provider_sample_interval_ms();

    (void)ctx;

    while (bStart && *bStart)
    {
        if (!s_gyro_ready)
        {
            usleep(100000);
            continue;
        }

        if (anj_gyro_provider_read(&data) != 0)
        {
            __ERR("gyro read failed\n");
            usleep(sample_interval_ms * 1000);
            continue;
        }

        anj_gyro_data_cache_update(&data);
        __DBG("accel(%.3f,%.3f,%.3f)  gyro(%.3f,%.3f,%.3f)dps  temp=%.1fC\n",
              data.accel_x, data.accel_y, data.accel_z,
              data.gyro_x, data.gyro_y, data.gyro_z, data.temp);
        usleep(sample_interval_ms * 1000);
    }

    return 0;
}

int anj_gyro_init(void)
{
    int iRet = 0;

    memset(&s_gyro_data, 0, sizeof(s_gyro_data));

    iRet = anj_gyro_provider_init();
    if (iRet < 0)
    {
        __ERR("gyro provider init failed: %d\n", iRet);
        return iRet;
    }
    if (iRet != 0)
    {
        __WARN("gyro provider init skipped\n");
        return 0;
    }

    s_gyro_ready = 1;

    strncpy(s_gyro_thread.iThreadName, "anj_gyro_sample_thread",
            sizeof(s_gyro_thread.iThreadName) - 1);
    s_gyro_thread.iThreadjob.func = anj_gyro_sample_thread;
    s_gyro_thread.iThreadjob.ctx = &s_gyro_thread;
    anj_thread_task_create(&s_gyro_thread);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_GYRO_DATA_GET, anj_gyro_data_event_get);

    return 0;
}

int anj_gyro_uninit(void)
{
    eventhub_unsubscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_GYRO_DATA_GET, anj_gyro_data_event_get);
    s_gyro_ready = 0;
    anj_thread_task_destroy(&s_gyro_thread, -1);
    anj_gyro_provider_uninit();

    anj_mutex_lock(&s_gyro_data_mutex);
    memset(&s_gyro_data, 0, sizeof(s_gyro_data));
    anj_mutex_unlock(&s_gyro_data_mutex);

    return 0;
}

REGISTER_MODULE(anj_gyro, MODULE_PRIORITY_GYRO);
