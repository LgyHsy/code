#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <asm/ioctl.h>

#include "anj_mw_time.h"
#include "anj_mw_mem.h"
#include "anj_mw_comm.h"
#include "anj_mw_thread.h"
#include "anj_mw_mutex.h"

#include "anj_mw_gpio.h"
#include "anj_mw_pwm.h"
#include "anj_mw_virtualdev.h"
#include "anj_mw_adc.h"
#include "anj_mw_ircut.h"
#include "anj_mw_gpiodev.h"
#include "anj_mw_i2c.h"

#include "anj_mw_hwctrl.h"

typedef enum
{
    HW_TASK_DELAY_FILP,     // 单次触发（延时关闭）
    HW_TASK_PERIOD_FILP,    // 周期性翻转（带结束时间）
} HwTaskType;

typedef enum
{
    HW_TYPE_GPIO,           // GPIO
    HW_TYPE_PWM_RLIGHT,     // 红外灯
    HW_TYPE_PWM_WLIGHT,     // 白光灯  
    HW_TYPE_IRCUT,          // IRCut
    HW_TYPE_EXPAND_GPIO_DEV,// 驱动扩展GPIO
} HwDeviceType;

typedef struct HwTask HwTask;
struct HwTask
{
    HwDeviceType dev_type;      // 设备类型
    HwTaskType etype;           // 任务类型
    int hw_id;                  // 硬件ID（gpio端口号等）
    int target_state;           // 目标状态

    uint64_t exec_time;     // 执行时间戳（绝对时间）
    int period_ms;          // 周期（ms）
    uint64_t end_time;      // 结束时间（绝对时间，0表示无限）
    uint64_t start_time;    // 开始时间（用于计算周期）

    HwTask  *next;
};

static pthread_mutex_t s_hwctrl_task_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static HwTask *s_hwctrl_task_queue = NULL;
static anj_thread_s s_stHwctrlSchThread;

static int s_LedGpioStatus = GPIO_VALUE_HIGH;   // 白光红外切换gpio状态，拉高是白光，拉低是红外

// 添加任务到队列
static void hwctrl_sch_task_enqueue(HwTask *new_task)
{
    anj_mutex_lock(&s_hwctrl_task_queue_mutex);

    if (s_hwctrl_task_queue == NULL || new_task->exec_time < s_hwctrl_task_queue->exec_time)
    {
        new_task->next = s_hwctrl_task_queue;
        s_hwctrl_task_queue = new_task;
    }
    else
    {
        HwTask *cur = s_hwctrl_task_queue;
        while (cur->next && cur->next->exec_time <= new_task->exec_time)
        {
            cur = cur->next;
        }

        new_task->next = cur->next;
        cur->next = new_task;
    }

    anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
}

static int anj_mw_hwctrl_sch_thread(void *ctx, int *bStart)
{
    int value = 0;
    int camera_idx = 0;

    while (bStart && *bStart)
    {
        anj_mutex_lock(&s_hwctrl_task_queue_mutex);
        uint64_t now = anj_mw_get_cputime_ms(NULL);
        
        HwTask **ptr = &s_hwctrl_task_queue;
        
        while (*ptr) 
        {
            HwTask *task = *ptr;

            if (task->exec_time > now)  // 任务未到期，检查下一个
            {
                ptr = &(*ptr)->next;
                continue;
            }
            
            *ptr = task->next;          // 任务到期 执行操作
            anj_mutex_unlock(&s_hwctrl_task_queue_mutex);

            // 执行硬件操作
            switch(task->dev_type) 
            {
                case HW_TYPE_GPIO:
                    anj_gpio_write_port_value(task->hw_id, task->target_state);
                    __INFO("hwctrl sch gpio port:%d set to value:%d\n", task->hw_id, task->target_state);
                    break;
                    
                case HW_TYPE_PWM_RLIGHT:
                    if (task->target_state != 0)
                        value = LIGHT_PWM_MAX_VALUE;
                    else
                        value = 0;

                    for (camera_idx = 0; camera_idx < ANJ_CAMERA_MAX_NUMS; camera_idx++)
                    {
                        anj_mw_hwctrl_pwm_rlight_set(camera_idx, value);
                    }
                    __INFO("hwctrl sch rlight set to value:%d\n", value);
                    break;
                    
                case HW_TYPE_PWM_WLIGHT:
                    if (task->target_state)
                        value = LIGHT_PWM_MAX_VALUE;
                    else
                        value = 0;

                    for (camera_idx = 0; camera_idx < ANJ_CAMERA_MAX_NUMS; camera_idx++)
                    {
                        anj_mw_hwctrl_pwm_wlight_set(camera_idx, value);
                    }
                    __INFO("hwctrl sch wlight set to:%d\n", value);
                    break;
                    
                case HW_TYPE_IRCUT:
                    if (task->target_state) 
                    {
                        anj_mw_hwctrl_ircut_set_night();
                        __INFO("hwctrl sch ircut set to night mode\n");
                    }
                    else
                    {
                        anj_mw_hwctrl_ircut_set_day();
                        __INFO("hwctrl sch ircut set to day mode\n");
                    }
                    break;
                case HW_TYPE_EXPAND_GPIO_DEV:
                    if (task->target_state)
                    {
                        if (task->hw_id == EXPAND_GPIO_PORT_ALARMLED)
                        {
                             anj_mw_hwctrl_alarmled_open();
                        }
                    }
                    else
                    {
                        if (task->hw_id == EXPAND_GPIO_PORT_ALARMLED)
                        {
                            anj_mw_hwctrl_alarmled_close();
                        }
                    }
                    break;
                    
                default:
                    __ERR("hwctrl sch unknown device type:%d\n", task->dev_type);
                    break;
            }

            if (task->etype == HW_TASK_DELAY_FILP)          // 单次翻转任务处理完释放
            {
                free(task);
            } 
            else if (task->etype == HW_TASK_PERIOD_FILP)    // 检查周期性任务是否已过期
            {
                if (task->end_time > 0 && now >= task->end_time) 
                {
                    __INFO("hwctrl sch period expired, dev_type:%d\n", task->dev_type);
                    free(task);
                }
                else
                {
                    task->exec_time += task->period_ms;
                    task->target_state = !task->target_state;
                    hwctrl_sch_task_enqueue(task);
                }
            }
            
            anj_mutex_lock(&s_hwctrl_task_queue_mutex);
            ptr = &s_hwctrl_task_queue;
            while (*ptr && (*ptr)->exec_time <= now)        // 跳过已处理的任务，从头开始重新扫描（因为队列可能已改变）
            {
                ptr = &(*ptr)->next;
            }
        }

        anj_mutex_unlock(&s_hwctrl_task_queue_mutex);

        // 动态休眠
        uint64_t sleep_ms = 10;
        anj_mutex_lock(&s_hwctrl_task_queue_mutex);
        if (s_hwctrl_task_queue)
        {
            uint64_t next_time = s_hwctrl_task_queue->exec_time;
            uint64_t now = anj_mw_get_cputime_ms(NULL);
            sleep_ms = (next_time > now) ? MIN(next_time - now, 100) : 0;
        }
        anj_mutex_unlock(&s_hwctrl_task_queue_mutex);

        usleep(sleep_ms * 1000);
    }

    return 0;
}

void anj_mw_hwctrl_sch_init()
{
    if (s_stHwctrlSchThread.start > 0)
    {
        __ERR("hwctrl sch already init\n");
        return;
    }

    memset(&s_stHwctrlSchThread, 0, sizeof(anj_thread_s));
    s_stHwctrlSchThread.bAutoDestroy = 0;
    snprintf(s_stHwctrlSchThread.iThreadName, sizeof(s_stHwctrlSchThread.iThreadName), "hwctrl_sch_thread");
    s_stHwctrlSchThread.iThreadjob.ctx = &s_stHwctrlSchThread;
    s_stHwctrlSchThread.iThreadjob.func = anj_mw_hwctrl_sch_thread;
    anj_thread_task_create(&s_stHwctrlSchThread);
}

void anj_mw_hwctrl_sch_uninit()
{
    anj_thread_task_destroy(&s_stHwctrlSchThread, 0);

    anj_mutex_lock(&s_hwctrl_task_queue_mutex);
    HwTask *task = s_hwctrl_task_queue;
    while (task)
    {
        HwTask *next = task->next;
        free(task);
        task = next;
    }
    s_hwctrl_task_queue = NULL;
    anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
}

static void hwctrl_sch_add_task(HwDeviceType dev_type,  // 任务类型
                                int hw_id,                      // 硬件ID（gpio类型时为具体端口）
                                int init_state,                 // 任务开始时的硬件初始状态（高或者低，打开或者关闭）
                                int delay_ms,                   // 任务首次执行需要的延时时间（立即执行或者马上执行）
                                int period_ms,                  // 任务操作一次的周期间隔时间（周期性操作一次）
                                int duration_ms)                // 任务总共持续时间
{
    int camera_idx = 0;
    // 立即设置初始状态
    switch(dev_type) 
    {
        case HW_TYPE_GPIO:
            anj_gpio_write_port_value(hw_id, init_state);
            break;
        case HW_TYPE_PWM_RLIGHT:
            for (camera_idx = 0; camera_idx < ANJ_CAMERA_MAX_NUMS; camera_idx++)
            {
                if (init_state)
                {
                    anj_mw_hwctrl_pwm_rlight_set(camera_idx, 100);
                }
                else
                {
                    anj_mw_hwctrl_pwm_rlight_set(camera_idx, 0);
                }
            }
            break;
        case HW_TYPE_PWM_WLIGHT:
            for (camera_idx = 0; camera_idx < ANJ_CAMERA_MAX_NUMS; camera_idx++)
            {
                if (init_state)
                {
                    anj_mw_hwctrl_pwm_wlight_set(camera_idx, 100);
                }
                else
                {
                    anj_mw_hwctrl_pwm_wlight_set(camera_idx, 0);
                }
            }
            break;
        case HW_TYPE_IRCUT:
            if (init_state)
            {
                anj_mw_hwctrl_ircut_set_day();
            }
            else
            {
                anj_mw_hwctrl_ircut_set_night();
            }
            break;
        case HW_TYPE_EXPAND_GPIO_DEV:
            if (init_state)
            {
                if (hw_id == EXPAND_GPIO_PORT_ALARMLED)
                {
                     anj_mw_hwctrl_alarmled_open();
                }
            }
            else
            {
                if (hw_id == EXPAND_GPIO_PORT_ALARMLED)
                {
                    anj_mw_hwctrl_alarmled_close();
                }
            }
            break;
        
        default:
            __ERR("hwctrl sch unknown device type:%d\n", dev_type);
            return;
    }

    HwTask *task = anj_mw_malloc(sizeof(HwTask));
    if (NULL == task)
    {
        __ERR("task malloc failed\n");
        return;
    }

    task->dev_type = dev_type;
    task->hw_id = hw_id;
    task->target_state = !init_state;  // 下次翻转的状态
    task->exec_time = anj_mw_get_cputime_ms(NULL) + delay_ms;
    
    if (period_ms > 0)      // 周期性任务
    {
        task->etype = HW_TASK_PERIOD_FILP;
        task->period_ms = period_ms;
        task->end_time = (duration_ms > 0) ? task->exec_time + duration_ms : 0;
    }
    else                    // 单次任务
    {
        task->etype = HW_TASK_DELAY_FILP;
        task->period_ms = 0;
        task->end_time = 0;
    }
    task->next = NULL;
    
    hwctrl_sch_task_enqueue(task);
    
    __INFO("hwctrl sch add task: dev_type:%d, hw_id:%d, init_state:%d, delay:%d, period:%d, duration:%d!\n",
           dev_type, hw_id, init_state, delay_ms, period_ms, duration_ms);
}

void hwctrl_sch_stop_task(HwDeviceType dev_type, int hw_id)
{
    anj_mutex_lock(&s_hwctrl_task_queue_mutex);

    HwTask **ptr = &s_hwctrl_task_queue;
    while (*ptr)
    {
        if ((*ptr)->dev_type == dev_type && (*ptr)->hw_id == hw_id) 
        {
            HwTask *to_free = *ptr;
            *ptr = (*ptr)->next;
            free(to_free);
        }
        else
        {
            ptr = &(*ptr)->next;
        }
    }
    anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
}


// 查找指定硬件的任务
static HwTask *hwctrl_sch_find_task(HwDeviceType dev_type, int hw_id)
{
    anj_mutex_lock(&s_hwctrl_task_queue_mutex);
    HwTask *task = s_hwctrl_task_queue;
    while (task)
    {
        if (task->dev_type == dev_type && task->hw_id == hw_id)
        {
            anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
            return task;
        }
        task = task->next;
    }
    anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
    return NULL;
}

// 修改任务时间
int hwctrl_sch_modify_task_time(HwDeviceType dev_type, int hw_id, int new_ms)
{
    HwTask *task = hwctrl_sch_find_task(dev_type, hw_id);
    if (!task)
    {
        return -1;
    }

    anj_mutex_lock(&s_hwctrl_task_queue_mutex);

    if (task->etype == HW_TASK_DELAY_FILP)
    {
        task->exec_time = anj_mw_get_cputime_ms(NULL) + new_ms;
    } 
    else if (task->etype == HW_TASK_PERIOD_FILP)
    {
        task->period_ms = new_ms;

        uint64_t now = anj_mw_get_cputime_ms(NULL);
        uint64_t elapsed = now - (task->exec_time - task->period_ms);
        task->exec_time = now + (new_ms - elapsed % new_ms);
    }

    anj_mutex_unlock(&s_hwctrl_task_queue_mutex);
    return 0;
}

// 获取任务剩余时间
int hwctrl_sch_get_task_remain_time(HwDeviceType dev_type, int hw_id)
{
    HwTask *task = hwctrl_sch_find_task(dev_type, hw_id);
    if (!task) 
    {
        return 0;
    }

    uint64_t now = anj_mw_get_cputime_ms(NULL);
    if (task->exec_time <= now)
    {
        return 0;
    }

    return (int)(task->exec_time - now);
}

void anj_mw_hwctrl_wlight_period_flicker(int period_ms, int duration_ms)
{
    int remain_time = hwctrl_sch_get_task_remain_time(HW_TYPE_PWM_WLIGHT, 0);
    if (remain_time == 0)
    {
        hwctrl_sch_add_task(HW_TYPE_PWM_WLIGHT, 0, 1, 0, period_ms, duration_ms);
    }
}

void anj_mw_hwctrl_alarmled_toggle_delay(int delayms)
{
    int hw_type = 0;
    int hw_id = 0;
    int init_state = 0;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        if (EXPAND_GPIO_PORT_ALARMLED <= 0)
        {
            __ERR("gpio alarm led don't support\n");
            return;
        }
    
        hw_type = HW_TYPE_EXPAND_GPIO_DEV;
        hw_id = EXPAND_GPIO_PORT_ALARMLED;
        init_state = EXPAND_GPIO_HIGH;
    }
    else
    {
        if (ANJ_GPIO_PORT_ALARM_LED <= 0)
        {
            __ERR("gpio alarm led don't support\n");
            return;
        }

        hw_type = HW_TYPE_GPIO;
        hw_id = ANJ_GPIO_PORT_ALARM_LED;
        init_state = GPIO_VALUE_LOW;
    }

    int remain_time = hwctrl_sch_get_task_remain_time(hw_type, hw_id);
    if (remain_time == 0)
    {
        hwctrl_sch_add_task(hw_type, hw_id, init_state, delayms, 0, 0);
    }
    else
    {
        hwctrl_sch_modify_task_time(hw_type, hw_id, delayms);
    }
}

int anj_mw_hwctrl_photo_sensor_get()
{
    int value = 0;

    if (ANJ_GPIO_PORT_PHOTO_SENSOR <= 0)
    {
        return GPIO_VALUE_LOW;
    }

    value = anj_gpio_read_port_value(ANJ_GPIO_PORT_PHOTO_SENSOR);
    value = value > 0 ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    /* 归一化：对外高=亮/白天，低=暗/夜晚（物理亮电平见 ANJ_PHOTO_SENSOR_BRIGHT_LEVEL） */
    return (value == ANJ_PHOTO_SENSOR_BRIGHT_LEVEL) ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
}

int anj_mw_hwctrl_photo_sensor_int()
{
    int iRet = -1;

    if (ANJ_GPIO_PORT_PHOTO_SENSOR <= 0)
    {
        __ERR("gpio photo sensor don't support!\n");
        return iRet;
    }

    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_PHOTO_SENSOR);
    if (iRet)
    {
        __ERR("init photo sensor gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_PHOTO_SENSOR, GPIO_DIREC_IN);
    if (iRet)
    {
        __ERR("init photo sensor gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_photo_sensor_uninit()
{
    int iRet = 0;

    if (ANJ_GPIO_PORT_PHOTO_SENSOR <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_PHOTO_SENSOR);
    return iRet;
}

int anj_mw_hwctrl_alarmled_open()
{
    int iRet = 0;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_alarm_led_ctrl(EXPAND_GPIO_HIGH);
    }
    else
    {
        if (ANJ_GPIO_PORT_ALARM_LED <= 0)
        {
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_ALARM_LED, GPIO_VALUE_LOW);
    }

    return iRet;
}

int anj_mw_hwctrl_alarmled_close()
{
    int iRet = 0;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_alarm_led_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_ALARM_LED <= 0)
        {
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_ALARM_LED, GPIO_VALUE_HIGH);
    }

    return iRet;
}

int anj_mw_hwctrl_alarmled_init()
{
    int iRet = -1;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        anj_gpio_expand_alarm_led_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_ALARM_LED <= 0)
        {
            __ERR("gpio alarm led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_ALARM_LED);
        if (iRet)
        {
            __ERR("init alarm led gpio failed!\n");
            return -1;
        }

        iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_ALARM_LED, GPIO_DIREC_OUT);
        if (iRet)
        {
            __ERR("init alarm led gpio failed!\n");
            return -1;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_ALARM_LED, GPIO_VALUE_HIGH);
        if (iRet)
        {
            __ERR("init alarm led gpio failed!\n");
            return -1;
        }
    }


    return iRet;
}

int anj_mw_hwctrl_alarmled_uninit()
{
    int iRet = -1;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        ;
    }
    else
    {
        if (ANJ_GPIO_PORT_ALARM_LED <= 0)
        {
            __ERR("gpio alarm led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_ALARM_LED);
    }

    return iRet;
}

int anj_mw_hwctrl_switch_simcard_status(int status)
{
    if (ANJ_GPIO_PORT_SWITCH_SIMCARD <= 0)
    {
        return 0;
    }
    int iValue = status ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    return anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_SIMCARD, iValue);
}

int anj_mw_hwctrl_switch_simcard_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_SWITCH_SIMCARD <= 0)
    {
        __ERR("gpio switch simcard don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_SWITCH_SIMCARD);
    if (iRet)
    {
        __ERR("init switch simcard gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_SWITCH_SIMCARD, GPIO_DIREC_OUT);
    if (iRet)
    {
        __ERR("init switch simcard gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_SIMCARD, GPIO_VALUE_LOW);
    if (iRet)
    {
        __ERR("init switch simcard gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_switch_simcard_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_SWITCH_SIMCARD <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_SWITCH_SIMCARD);
    return iRet;
}

int anj_mw_hwctrl_aov_4g_resume_get(int status)
{
    if (ANJ_GPIO_PORT_AOV_4G_RESUME <= 0)
    {
        return 0;
    }

    return anj_gpio_read_port_value(ANJ_GPIO_PORT_AOV_4G_RESUME);
}

int anj_mw_hwctrl_aov_4g_resume_status(int status)
{
    if (ANJ_GPIO_PORT_AOV_4G_RESUME <= 0)
    {
        return 0;
    }

    int iValue = status ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    return anj_gpio_write_port_value(ANJ_GPIO_PORT_AOV_4G_RESUME, iValue);
}

int anj_mw_hwctrl_aov_4g_resume_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_AOV_4G_RESUME <= 0)
    {
        __ERR("gpio aov 4g resume don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_AOV_4G_RESUME);
    if (iRet)
    {
        __ERR("init aov 4g resume gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_AOV_4G_RESUME, GPIO_DIREC_OUT);
    if (iRet)
    {
        __ERR("init aov 4g resume gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_AOV_4G_RESUME, GPIO_VALUE_LOW);
    if (iRet)
    {
        __ERR("init aov 4g resume gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_aov_4g_resume_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_AOV_4G_RESUME <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_AOV_4G_RESUME);
    return iRet;
}

int anj_mw_hwctrl_aov_4g_stat_get()
{
    if (ANJ_GPIO_PORT_AOV_4G_STAT <= 0)
    {
        return 0;
    }

    return anj_gpio_read_port_value(ANJ_GPIO_PORT_AOV_4G_STAT);
}

int anj_mw_hwctrl_aov_4g_stat_init()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_AOV_4G_STAT <= 0)
    {
        __ERR("gpio aov 4g stat don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_AOV_4G_STAT);
    if (iRet)
    {
        __ERR("init aov 4g stat gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_AOV_4G_STAT, GPIO_DIREC_IN);
    if (iRet)
    {
        __ERR("init aov 4g stat gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_aov_4g_stat_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_AOV_4G_STAT <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_AOV_4G_STAT);
    return iRet;
}

int anj_mw_hwctrl_aov_charge_get()
{
    if (ANJ_GPIO_PORT_AOV_CHARGE <= 0)
    {
        return 0;
    }

    return anj_gpio_read_port_value(ANJ_GPIO_PORT_AOV_CHARGE);
}

int anj_mw_hwctrl_aov_charge_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_AOV_CHARGE <= 0)
    {
        __ERR("gpio aov charge don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_AOV_CHARGE);
    if (iRet)
    {
        __ERR("init aov charge gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_AOV_CHARGE, GPIO_DIREC_IN);
    if (iRet)
    {
        __ERR("init aov charge gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_aov_charge_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_AOV_CHARGE <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_AOV_CHARGE);
    return iRet;
}

int anj_mw_hwctrl_usb_status(int status)
{
    if (ANJ_GPIO_PORT_USB_POWER <= 0)
    {
        return 0;
    }

    int iValue = status ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    return anj_gpio_write_port_value(ANJ_GPIO_PORT_USB_POWER, iValue);
}

int anj_mw_hwctrl_usb_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_USB_POWER <= 0)
    {
        __ERR("gpio usb don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_USB_POWER);
    if (iRet)
    {
        __ERR("init usb gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_USB_POWER, GPIO_DIREC_OUT);
    if (iRet)
    {
        __ERR("init usb gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_USB_POWER, IPC_USB_GPIO_DEFAULT_VALUE);
    if (iRet)
    {
        __ERR("init usb gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_usb_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_USB_POWER <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_USB_POWER);
    return iRet;
}

int anj_mw_hwctrl_gyro_power_status(int status)
{
    if (ANJ_GPIO_PORT_GYRO_POWER <= 0)
    {
        return 0;
    }

    int iValue = status ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    return anj_gpio_write_port_value(ANJ_GPIO_PORT_GYRO_POWER, iValue);
}

int anj_mw_hwctrl_gyro_power_init()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_GYRO_POWER <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_GYRO_POWER);
    if (iRet)
    {
        __ERR("init gyro power gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_GYRO_POWER, GPIO_DIREC_OUT);
    if (iRet)
    {
        __ERR("init gyro power gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_GYRO_POWER, GPIO_VALUE_LOW);
    if (iRet)
    {
        __ERR("init gyro power gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_gyro_power_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_GYRO_POWER <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_GYRO_POWER);
    return iRet;
}

int anj_mw_hwctrl_reset_get()
{
    if (ANJ_GPIO_PORT_RESET <= 0)
    {
        return -1;
    }

    return anj_gpio_read_port_value(ANJ_GPIO_PORT_RESET);
}

int anj_mw_hwctrl_reset_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_RESET <= 0)
    {
        __ERR("gpio reset don't support!\n");
        return iRet;
    }
    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_RESET);
    if (iRet)
    {
        __ERR("init reset gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_RESET, GPIO_DIREC_IN);
    if (iRet)
    {
        __ERR("init reset gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_mw_hwctrl_reset_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_RESET <= 0)
    {
        return iRet;
    }
    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_RESET);
    return iRet;
}

int anj_mw_hwctrl_switch_ao_open()
{
    int iRet = 0;
    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_ao_ctrl(EXPAND_GPIO_HIGH);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_AUDIOOUT <= 0)
        {
            __ERR("gpio switch ao don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_AUDIOOUT, GPIO_VALUE_HIGH);
    }

    return iRet;
}

int anj_mw_hwctrl_switch_ao_close()
{
    int iRet = 0;

    if (ANJ_EXPAND_DEV_GPIO)
    {
        iRet = anj_gpio_expand_switch_ao_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_AUDIOOUT <= 0)
        {
            __ERR("gpio switch ao don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_AUDIOOUT, GPIO_VALUE_LOW);
    }

    return iRet;
}

int anj_mw_hwctrl_switch_ao_init()
{
    int iRet = -1;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        anj_gpio_expand_switch_ao_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_AUDIOOUT <= 0)
        {
            __ERR("gpio switch audio out don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_SWITCH_AUDIOOUT);
        if (iRet)
        {
            __ERR("init swicth audio out gpio failed!\n");
            return -1;
        }

        iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_SWITCH_AUDIOOUT, GPIO_DIREC_OUT);
        if (iRet)
        {
            __ERR("init swicth audio out gpio failed!\n");
            return -1;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_AUDIOOUT, GPIO_VALUE_LOW);
        if (iRet)
        {
            __ERR("init swicth audio out gpio failed!\n");
            return -1;
        }
    }

    return iRet;
}

int anj_mw_hwctrl_switch_ao_uninit()
{
    int iRet = 0;
    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_ao_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_AUDIOOUT <= 0)
        {
            __ERR("gpio switch ao don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_SWITCH_AUDIOOUT);
    }

    return iRet;
}

int anj_mw_hwctrl_switch_led_get()
{
    return s_LedGpioStatus;
}

int anj_mw_hwctrl_switch_wled()
{
    if (s_LedGpioStatus == GPIO_VALUE_HIGH)
        return 0;

    int iRet = 0;
    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_led_ctrl(EXPAND_GPIO_HIGH);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_LED <= 0)
        {
            __ERR("gpio switch led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_LED, GPIO_VALUE_HIGH);
    }

    if (iRet == 0)
        s_LedGpioStatus = GPIO_VALUE_HIGH;

    return iRet;
}

int anj_mw_hwctrl_switch_rled()
{
    if (s_LedGpioStatus == GPIO_VALUE_LOW)
    {
        return 0;
    }

    int iRet = 0;
    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_led_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_LED <= 0)
        {
            __ERR("gpio switch led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_LED, GPIO_VALUE_LOW);
    }

    if (iRet == 0)
    {
        s_LedGpioStatus = GPIO_VALUE_LOW;
    }

    return iRet;
}

int anj_mw_hwctrl_switch_led_init()
{
    int iRet = -1;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_led_ctrl(EXPAND_GPIO_HIGH);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_LED <= 0)
        {
            __ERR("gpio switch led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_SWITCH_LED);
        if (iRet)
        {
            __ERR("init switch led gpio failed!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_SWITCH_LED, GPIO_DIREC_OUT);
        if (iRet)
        {
            __ERR("init switch led gpio failed!\n");
            return iRet;
        }

        int value = GPIO_VALUE_LOW;
        if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE || ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE_RED)
        {
            value = GPIO_VALUE_HIGH;
        }
        else
        {
            value = GPIO_VALUE_LOW;
        }

        s_LedGpioStatus = value;
        iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_SWITCH_LED, value);
        if (iRet)
        {
            __ERR("init switch led gpio failed!\n");
            return iRet;
        }
    }

    return iRet;
}

int anj_mw_hwctrl_switch_led_uninit()
{
    int iRet = 0;

    if (ANJ_EXPAND_DEV_GPIO > 0)
    {
        iRet = anj_gpio_expand_switch_led_ctrl(EXPAND_GPIO_LOW);
    }
    else
    {
        if (ANJ_GPIO_PORT_SWITCH_LED <= 0)
        {
            __ERR("gpio switch led don't support!\n");
            return iRet;
        }

        iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_SWITCH_LED);
    }

    return iRet;
}

int anj_mw_hwctrl_alarmout_get_port(int chn)
{
    int gpio_port = 0;

    if (E_GPIO_ALARM_CHN1 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
    }

    return gpio_port;
}

void anj_mw_hwctrl_alarmout_toggle_delay(int chn, int state, int delayms)
{
    int port = anj_mw_hwctrl_alarmout_get_port(chn);
    if (port > 0)
    {
        // 如果不存在任务则创建任务，如果任务剩余时间>0则更新下一次执行时间
        int remain_time = hwctrl_sch_get_task_remain_time(HW_TYPE_GPIO, port);
        if (remain_time == 0)
        {
            hwctrl_sch_add_task(HW_TYPE_GPIO, port, state, delayms, 0, 0);
        }
        else
        {
            hwctrl_sch_modify_task_time(HW_TYPE_GPIO, port, delayms);
        }
    }
}

int anj_mw_hwctrl_alarmout_init(int chn, const char *pStatus)
{
    int iRet = 0;
    int gpio_status = GPIO_VALUE_HIGH;
    int gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;

    if (pStatus == NULL)
    {
        return -1;
    }

    // 初始化状态为触发状态取反：HIGH(常开)常态写0，否则常态写1
    if (strcmp(pStatus, ANJ_GPIO_LEVEL_STR_HIGH) == 0)
    {
        gpio_status = GPIO_VALUE_LOW;
    }
    else
    {
        gpio_status = GPIO_VALUE_HIGH;
    }

    if (E_GPIO_ALARM_CHN1 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
    }

    if (gpio_port > 0)
    {
        iRet = anj_gpio_write_port_export(gpio_port);
        if (iRet)
        {
            __ERR("gpio init alarmout chn:%d gpio:%d export failed!\n", chn, gpio_port);
            return -1;
        }

        iRet = anj_gpio_write_port_direc(gpio_port, GPIO_DIREC_OUT);
        if (iRet)
        {
            __ERR("gpio init alarmout chn:%d gpio:%d direction failed!\n", chn, gpio_port);
            return -1;
        }

        iRet = anj_gpio_write_port_value(gpio_port, gpio_status);
        if (iRet)
        {
            __ERR("gpio init alarmout chn:%d gpio:%d vaule:%d failed!\n", chn, gpio_port, gpio_status);
            return -1;
        }
    }
    else
    {
        __INFO("gpio init alarmout chn:%d but gpio is %d, please check!\n", chn, gpio_port);
    }

    return 0;
}

int anj_mw_hwctrl_alarmout_uninit(int chn)
{
    int iRet = 0;
    int gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;

    if (E_GPIO_ALARM_CHN1 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
    }

    if (gpio_port > 0)
    {
        iRet = anj_gpio_write_port_unexport(gpio_port);
    }

    return iRet;
}

int anj_mw_hwctrl_alarmout_port_count_get()
{
    int count = 0;
    if (ANJ_GPIO_PORT_ALARMOUT_CHN1 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMOUT_CHN2 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMOUT_CHN3 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMOUT_CHN4 > 0)
        count++;

    return count;
}

int anj_mw_hwctrl_alarmout_usabale_port_get()
{
    int chn = 0;
    int gpio_port = 0;

    for (chn = E_GPIO_ALARM_CHN1; chn < E_GPIO_ALARM_CHN_MAX; chn++)
    {
        if (chn == E_GPIO_ALARM_CHN1 && ANJ_GPIO_PORT_ALARMOUT_CHN1 > 0)
        {
            gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
            break;
        }
        else if (chn == E_GPIO_ALARM_CHN2 && ANJ_GPIO_PORT_ALARMOUT_CHN2 > 0)
        {
            gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
            break;
        }
        else if (chn == E_GPIO_ALARM_CHN3 && ANJ_GPIO_PORT_ALARMOUT_CHN3 > 0)
        {
            gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
            break;
        }
        else if (chn == E_GPIO_ALARM_CHN4 && ANJ_GPIO_PORT_ALARMOUT_CHN4 > 0)
        {
            gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
            break;
        }
    }

    return gpio_port;
}

int anj_mw_hwctrl_alarmout_usable_chn_status_get()
{
    int gpio_port = 0;
    int gpio_status = -1;

    gpio_port = anj_mw_hwctrl_alarmout_usabale_port_get();
    if (gpio_port > 0)
    {
        gpio_status = anj_gpio_read_port_value(gpio_port);
    }

    return gpio_status;
}
int anj_mw_hwctrl_alarmout_usable_chn_status_set(int status)
{
    int gpio_port = 0;
    gpio_port = anj_mw_hwctrl_alarmout_usabale_port_get();
    if (gpio_port > 0)
    {
        anj_gpio_write_port_value(gpio_port, status);
    }

    return 0;
}

int anj_mw_hwctrl_alarmout_chn_status_get(int chn)
{
    int gpio_port = 0;
    int gpio_status = -1;

    if (E_GPIO_ALARM_CHN1 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
    }

    if (gpio_port > 0)
    {
        gpio_status = anj_gpio_read_port_value(gpio_port);
    }

    return gpio_status;
}

int anj_mw_hwctrl_alarmout_chn_status_set(int chn, int status)
{
    int iRet = -1;
    int gpio_port = 0;
    int gpio_status = (status == 0) ? 0 : 1;

    if (E_GPIO_ALARM_CHN1 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        gpio_port = ANJ_GPIO_PORT_ALARMOUT_CHN4;
    }

    if (gpio_port > 0)
    {
        iRet = anj_gpio_write_port_value(gpio_port, gpio_status);
    }

    return iRet;
}

static int alarmin_get_port(int chn)
{
    if (E_GPIO_ALARM_CHN1 == chn)
    {
        return ANJ_GPIO_PORT_ALARMIN_CHN1;
    }
    else if (E_GPIO_ALARM_CHN2 == chn)
    {
        return ANJ_GPIO_PORT_ALARMIN_CHN2;
    }
    else if (E_GPIO_ALARM_CHN3 == chn)
    {
        return ANJ_GPIO_PORT_ALARMIN_CHN3;
    }
    else if (E_GPIO_ALARM_CHN4 == chn)
    {
        return ANJ_GPIO_PORT_ALARMIN_CHN4;
    }
    return 0;
}

static int alarmin_chn_gpio_init(int chn)
{
    int iRet = 0;
    int gpio_port = alarmin_get_port(chn);

    if (gpio_port > 0)
    {
        iRet = anj_gpio_write_port_export(gpio_port);
        if (iRet)
        {
            __ERR("gpio init alarmin chn:%d gpio:%d export failed!\n", chn, gpio_port);
            return -1;
        }

        iRet = anj_gpio_write_port_direc(gpio_port, GPIO_DIREC_IN);
        if (iRet)
        {
            __ERR("gpio init alarmin chn:%d gpio:%d direction failed!\n", chn, gpio_port);
            return -1;
        }
    }
    else
    {
        __INFO("gpio init alarmin chn:%d but gpio is %d, please check!\n", chn, gpio_port);
    }

    return 0;
}

static int alarmin_chn_gpio_uninit(int chn)
{
    int gpio_port = alarmin_get_port(chn);

    if (gpio_port > 0)
    {
        return anj_gpio_write_port_unexport(gpio_port);
    }

    return 0;
}

int anj_mw_hwctrl_alarmin_port_count_get()
{
    int count = 0;
    if (ANJ_GPIO_PORT_ALARMIN_CHN1 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMIN_CHN2 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMIN_CHN3 > 0)
        count++;
    if (ANJ_GPIO_PORT_ALARMIN_CHN4 > 0)
        count++;

    return count;
}

int anj_mw_hwctrl_alarmin_chn_status_get(int chn)
{
    int gpio_port = alarmin_get_port(chn);
    int gpio_status = -1;

    if (gpio_port <= 0)
    {
        return -1;
    }

    gpio_status = anj_gpio_read_port_value(gpio_port);
    if (gpio_status < 0)
    {
        return -1;
    }

    gpio_status = (gpio_status > 0) ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
    /* 归一化：对外高=闭合、低=断开（物理闭合电平见 ANJ_ALARMIN_CLOSE_LEVEL） */
    return (gpio_status == ANJ_ALARMIN_CLOSE_LEVEL) ? GPIO_VALUE_HIGH : GPIO_VALUE_LOW;
}

int anj_mw_hwctrl_alarmin_init(void)
{
    int chn = 0;

    if (anj_mw_hwctrl_alarmin_port_count_get() <= 0)
    {
        return 0;
    }

    for (chn = E_GPIO_ALARM_CHN1; chn < E_GPIO_ALARM_CHN_MAX; chn++)
    {
        if (alarmin_get_port(chn) <= 0)
        {
            continue;
        }
        alarmin_chn_gpio_init(chn);
    }
    return 0;
}

int anj_mw_hwctrl_alarmin_uninit(void)
{
    int chn = 0;

    for (chn = E_GPIO_ALARM_CHN1; chn < E_GPIO_ALARM_CHN_MAX; chn++)
    {
        if (alarmin_get_port(chn) <= 0)
        {
            continue;
        }
        alarmin_chn_gpio_uninit(chn);
    }
    return 0;
}

int anj_mw_hwctrl_expand_gpio_dev_init()
{
    return anj_gpio_expand_dev_init();
}

void anj_mw_hwctrl_expand_gpio_dev_uninit()
{
    anj_gpio_expand_dev_uninit();
}

void anj_mw_hwctrl_init()
{
#if ANJ_EXPAND_DEV_GPIO
    anj_mw_hwctrl_expand_gpio_dev_init();
#endif

    anj_mw_hwctrl_reset_init();
    anj_mw_hwctrl_photo_sensor_int();
    anj_mw_hwctrl_switch_ao_init();
    anj_mw_hwctrl_switch_led_init();
    anj_mw_hwctrl_alarmled_init();
    anj_mw_hwctrl_switch_simcard_init();
#ifdef _USE_MODULE_AOV_
    anj_mw_hwctrl_aov_4g_resume_init();
    anj_mw_hwctrl_aov_4g_stat_init();
    anj_mw_hwctrl_aov_charge_init();
#endif
    anj_mw_hwctrl_sch_init();
    anj_mw_hwctrl_usb_init();
    anj_mw_hwctrl_gyro_power_init();
}

void anj_mw_hwctrl_uninit()
{
    anj_mw_hwctrl_gyro_power_uninit();
    anj_mw_hwctrl_usb_uninit();
    anj_mw_hwctrl_sch_uninit();

    anj_mw_hwctrl_photo_sensor_uninit();
    anj_mw_hwctrl_reset_uninit();
    anj_mw_hwctrl_switch_ao_uninit();
    anj_mw_hwctrl_switch_led_uninit();
    anj_mw_hwctrl_alarmled_uninit();
    anj_mw_hwctrl_switch_simcard_uninit();
#ifdef _USE_MODULE_AOV_
    anj_mw_hwctrl_aov_charge_uninit();
    anj_mw_hwctrl_aov_4g_resume_uninit();
    anj_mw_hwctrl_aov_4g_stat_uninit();
#endif

#if ANJ_EXPAND_DEV_GPIO
    anj_mw_hwctrl_expand_gpio_dev_uninit();
#endif

}

int anj_mw_hwctrl_pwm_export(int chipnum, int chnnum)
{
    int iRet = 0;
    if (chipnum < 0 || chnnum < 0)
    {
        __ERR("error chipnum:%d chnnum:%d\n", chipnum, chipnum);
        return iRet;
    }

    iRet = anj_mw_pwm_export(chipnum, chipnum);
    return iRet;
}

int anj_mw_hwctrl_pwm_set_period(int chipnum, int chnnum, int period_ns)
{
    int iRet = 0;
    if (period_ns < 0 || (chipnum < 0 || chnnum < 0))
    {
        __ERR("error chipnum:%d chnnum:%d or period_ns:%d\n", chipnum, chipnum, period_ns);
        return -1;
    }

    iRet = anj_mw_pwm_set_period(chipnum, chipnum, period_ns);
    return iRet;
}

int anj_mw_hwctrl_pwm_set_duty_cycle(int chipnum, int chnnum, int duty_ns)
{
    int iRet = 0;
    if (duty_ns < 0 || (chipnum < 0 || chnnum < 0))
    {
        __ERR("error chipnum:%d chnnum:%d or duty_ns:%d\n", chipnum, chipnum, duty_ns);
        return -1;
    }

    iRet = anj_mw_pwm_set_duty_cycle(chipnum, chnnum, duty_ns);
    return iRet;
}

// 设置PWM极性(normal或inversed)
int anj_mw_hwctrl_pwm_set_polarity(int chipnum, int chnnum, const char *polarity)
{
    int iRet = 0;
    if (polarity == NULL || (chipnum < 0 || chnnum < 0))
    {
        __ERR("error chipnum:%d chnnum:%d or polarity\n", chipnum, chipnum);
        return -1;
    }

    iRet = anj_mw_pwm_set_polarity(chipnum, chipnum, polarity);
    return iRet;
}

int anj_mw_hwctrl_pwm_set_enable(int chipnum, int chnnum, int enable)
{
    int iRet = 0;
    if (chipnum < 0 || chnnum < 0)
    {
        __ERR("error chipnum:%d chnnum:%d\n", chipnum, chipnum);
        return iRet;
    }

    iRet = anj_mw_pwm_set_enable(chipnum, chnnum, enable);
    return iRet;
}

int anj_mw_hwctrl_pwm_init(int chipnum, int chnnum, int period, int dutycycle)
{
    int iRet = 0;
    if (chipnum < 0)
    {
        __ERR("error pwm_no:%d\n", chipnum);
        return iRet;
    }

    // 1.export
    iRet = anj_mw_pwm_export(chipnum, chnnum);
    if (iRet)
    {
        __ERR("pwm export chipnum:%d chnnum:%d failed\n", chipnum, chnnum);
        return -1;
    }

    // 2. set period
    iRet = anj_mw_pwm_set_period(chipnum, chnnum, period);
    if (iRet)
    {
        __ERR("pwm set chipnum:%d chnnum:%d period:%d failed\n", chipnum, chnnum, period);
        return -1;
    }

    // 3.set dutycycle
    iRet = anj_mw_pwm_set_duty_cycle(chipnum, chnnum, dutycycle);
    if (iRet)
    {
        __ERR("pwm set chipnum:%d chnnum:%d duty cycle:%d failed\n", chipnum, chnnum, dutycycle);
        return -1;
    }

    // 4.set enable
    iRet = anj_mw_pwm_set_enable(chipnum, chnnum, 0);
    if (iRet)
    {
        __ERR("pwm set chipnum:%d chnnum:%d enable failed\n", chipnum, chnnum);
        return -1;
    }

    return 0;
}

int anj_mw_hwctrl_pwm_uninit(int chipnum, int chnnum)
{
    int iRet = 0;
    if (chipnum < 0)
    {
        __ERR("error pwm_no:%d\n", chipnum);
        return iRet;
    }

    iRet = anj_mw_pwm_unexport(chipnum, chnnum);
    __ERR("uninit chipnum:%d chnnum:%d %s\n", chipnum, chnnum, (iRet == 0) ? "success" : "failed");

    return iRet;
}

int anj_mw_hwctrl_pwm_set_grp_period(int pwm_id, int group, int period)
{
    return anj_mw_pwm_set_group_period(pwm_id, group, period);
}

int anj_mw_hwctrl_pwm_set_grp_shift(int pwm_id, int group, int shift)
{
    return anj_mw_pwm_set_group_shift(pwm_id, group, shift);
}

int anj_mw_hwctrl_pwm_set_grp_duty(int pwm_id, int group, int duty)
{
    return anj_mw_pwm_set_group_duty(pwm_id, group, duty);
}

int anj_mw_hwctrl_pwm_set_grp_polarity(int pwm_id, int group, int polarity)
{
    return anj_mw_pwm_set_group_polarity(pwm_id, group, polarity);
}

int anj_mw_hwctrl_pwm_set_grp_low_idle(int enable, int group)
{
    return anj_mw_pwm_set_group_low_idle(enable, group);
}

int anj_mw_hwctrl_pwm_set_grp_enable(int enable, int group)
{
    return anj_mw_pwm_set_group_enable(enable, group);
}

int anj_mw_hwctrl_pwm_set_grp_round(int step, int group)
{
    return anj_mw_pwm_set_group_round(step, group);
}

void anj_mw_hwctrl_motor_init()
{
    anj_virdev_motor_init();
}

void anj_mw_hwctrl_motor_uninit()
{
    anj_virdev_motor_uninit();
}

void anj_mw_hwctrl_motor_up(int step, int period)
{
    anj_virdev_motor_up(step, period);
}

void anj_mw_hwctrl_motor_down(int step, int period)
{
    anj_virdev_motor_down(step, period);
}
void anj_mw_hwctrl_motor_left(int step, int period)
{
    anj_virdev_motor_left(step, period);
}

void anj_mw_hwctrl_motor_right(int step, int period)
{
    anj_virdev_motor_right(step, period);
}

int anj_mw_hwctrl_motor_remain_step()
{
    return anj_virdev_motor_get_group_round();
}

int anj_mw_hwctrl_motor_stop()
{
    return anj_virdev_motor_stop();
}

int anj_mw_hwctrl_pwm_gpio_ctrl(int cmd)
{
    return anj_mw_pwm_gpio_ctrl(cmd);
}

int anj_mw_hwctrl_get_adc_value()
{
    return 0;
}

int anj_mw_hwctrl_pwm_rlight_set(int camera, int value)
{
    int iRet = 0;
    int port = ANJ_PWM_LIGHT_PORT_R;

    if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE)
    {
        return 0;
    }

    if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        port = ANJ_PWM_LIGHT_PORT2;
    }

    if (ANJ_GPIO_PORT_SWITCH_LED > 0 || ANJ_EXPAND_DEV_GPIO > 0)
    {
        anj_mw_hwctrl_switch_rled();
    }

    if (value > 0)
    {
        iRet = anj_mw_pwm_set_duty_cycle(0, port, value);
        if (iRet)
        {
            return iRet;
        }
        return anj_mw_pwm_set_enable(0, port, 1);
    }

    iRet = anj_mw_pwm_set_duty_cycle(0, port, 0);
    if (iRet)
    {
        return iRet;
    }
    return anj_mw_pwm_set_enable(0, port, 0);
}

int anj_mw_hwctrl_pwm_rlight_get(int camera)
{
    int value = 0;
    int port = ANJ_PWM_LIGHT_PORT_R;

    if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_WHITE)
    {
        return value;
    }

    if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        port = ANJ_PWM_LIGHT_PORT2;
    }

    if ((ANJ_GPIO_PORT_SWITCH_LED > 0 || ANJ_EXPAND_DEV_GPIO > 0)
        && anj_mw_hwctrl_switch_led_get() != GPIO_VALUE_LOW)
    {
        return value;
    }

    anj_mw_pwm_get_duty_cycle(0, port, &value);
    return value;
}

int anj_mw_hwctrl_pwm_wlight_set(int camera, int value)
{
    int iRet = 0;
    int port = ANJ_PWM_LIGHT_PORT_W;

    if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_RED)
    {
        return 0;
    }

    if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        port = ANJ_PWM_LIGHT_PORT2;
    }

    if (ANJ_GPIO_PORT_SWITCH_LED > 0 || ANJ_EXPAND_DEV_GPIO > 0)
    {
        anj_mw_hwctrl_switch_wled();
    }

    if (value > 0)
    {
        iRet = anj_mw_pwm_set_duty_cycle(0, port, value);
        if (iRet)
        {
            return iRet;
        }
        return anj_mw_pwm_set_enable(0, port, 1);
    }

    iRet = anj_mw_pwm_set_duty_cycle(0, port, 0);
    if (iRet)
    {
        return iRet;
    }
    return anj_mw_pwm_set_enable(0, port, 0);
}

int anj_mw_hwctrl_pwm_wlight_get(int camera)
{
    int value = 0;
    int port = ANJ_PWM_LIGHT_PORT_W;

    if (ANJ_IPC_LIGHT_TYPE == LIGHTBOARD_TYPE_RED)
    {
        return value;
    }

    if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        port = ANJ_PWM_LIGHT_PORT2;
    }

    if ((ANJ_GPIO_PORT_SWITCH_LED > 0 || ANJ_EXPAND_DEV_GPIO > 0)
        && anj_mw_hwctrl_switch_led_get() != GPIO_VALUE_HIGH)
    {
        return value;
    }

    anj_mw_pwm_get_duty_cycle(0, port, &value);
    return value;
}

int anj_mw_hwctrl_pwm_light_init(int camera, int period, int duty_cycle)
{
    int iRet = 0;

    if (camera == 0)
    {
        iRet = anj_mw_hwctrl_pwm_init(0, ANJ_PWM_LIGHT_PORT_R, period, duty_cycle);
        if (ANJ_PWM_LIGHT_PORT_R != ANJ_PWM_LIGHT_PORT_W)
        {
            iRet = anj_mw_hwctrl_pwm_init(0, ANJ_PWM_LIGHT_PORT_W, period, duty_cycle);
        }
    }
    else if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        iRet = anj_mw_hwctrl_pwm_init(0, ANJ_PWM_LIGHT_PORT2, period, duty_cycle);
    }

    return iRet;
}

int anj_mw_hwctrl_pwm_light_uninit(int camera)
{
    int iRet = 0;

    if (camera == 0)
    {
        iRet = anj_mw_hwctrl_pwm_uninit(0, ANJ_PWM_LIGHT_PORT_R);
        if (ANJ_PWM_LIGHT_PORT_R != ANJ_PWM_LIGHT_PORT_W)
        {
            iRet = anj_mw_hwctrl_pwm_uninit(0, ANJ_PWM_LIGHT_PORT_W);
        }
    }
    else if (camera == 1 && ANJ_PWM_LIGHT_PORT2 > 0)
    {
        iRet = anj_mw_hwctrl_pwm_uninit(0, ANJ_PWM_LIGHT_PORT2);
    }

    return iRet;
}

int anj_mw_hwctrl_ircut_init()
{
    int iRet = 0;
    iRet = anj_mw_ircut_init();
    return iRet;
}

int anj_mw_hwctrl_ircut_uninit()
{
    int iRet = 0;
    iRet = anj_mw_ircut_uninit();
    return iRet;
}

int anj_mw_hwctrl_ircut_set_day()
{
    int iRet = 0;
    iRet = anj_mw_ircut_set(IRCUT_DAY);
    return iRet;
}

int anj_mw_hwctrl_ircut_set_night()
{
    int iRet = 0;
    iRet = anj_mw_ircut_set(IRCUT_NIGHT);
    return iRet;
}

int anj_mw_hwctrl_batteryadc_init()
{
    int iRet = 0;
    iRet = anj_mw_adc_battery_init();
    return iRet;
}

int anj_mw_hwctrl_batteryadc_uninit()
{
    int iRet = 0;
    iRet = anj_mw_adc_battery_uninit();
    return iRet;
}

int anj_mw_hwctrl_batteryadc_get()
{
    int value = 0;
    value = anj_mw_adc_battery_get();
    return value;
}

