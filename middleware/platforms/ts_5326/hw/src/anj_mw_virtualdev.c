#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "anj_mw_comm.h"
#include "anj_mw_gpio.h"
#include "anj_mw_virtualdev.h"

#define DEFAULT_PERIOD (20000000)    /*单位HZ 默认水平电机速度是16ms一步*/
#define MOTOR_DIRECTION_FORWARD (0)  /*云台转动方向 0向前转*/
#define MOTOR_DIRECTION_BACKWARD (1) /*云台转动方向 1向后转*/

#define MOTOR_WAIT_STOP_STEP (30)          /*再运行步数停止*/
#define MOTOR_WAIT_TIME (100 * 1000)       /*等待100MS*/
#define MOTOR_OVER_TIME (60 * 1000 * 1000) /*1分钟云台阻塞超时*/

typedef struct PtzRunParam
{
    int Step; /*电机实际要转动的步数*/
    int bRun;
    int Period;
} MotorRunParam;
static MotorRunParam s_stMotorRunParam = {0};

int anj_virdev_motor_select_v()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_MOTOT_SEL <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_MOTOT_SEL, GPIO_VALUE_LOW);
    return iRet;
}

int anj_virdev_motor_select_h()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_MOTOT_SEL <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_MOTOT_SEL, GPIO_VALUE_HIGH);
    return iRet;
}

int anj_virdev_motor_select_init()
{
    int iRet = -1;
    if (ANJ_GPIO_PORT_MOTOT_SEL <= 0)
    {
        __ERR("gpio motor select don't support!\n");
        return iRet;
    }

    iRet = anj_gpio_write_port_export(ANJ_GPIO_PORT_MOTOT_SEL);
    if (iRet)
    {
        __ERR("init motor select gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_direc(ANJ_GPIO_PORT_MOTOT_SEL, GPIO_DIREC_OUT);
    if (iRet)
    {
        __ERR("init motor select gpio failed!\n");
        return -1;
    }

    iRet = anj_gpio_write_port_value(ANJ_GPIO_PORT_MOTOT_SEL, GPIO_VALUE_HIGH);
    if (iRet)
    {
        __ERR("init motor select gpio failed!\n");
        return -1;
    }

    return iRet;
}

int anj_virdev_motor_select_uninit()
{
    int iRet = 0;
    if (ANJ_GPIO_PORT_MOTOT_SEL <= 0)
    {
        return iRet;
    }

    iRet = anj_gpio_write_port_unexport(ANJ_GPIO_PORT_MOTOT_SEL);
    return iRet;
}

int anj_virdev_motor_set_group_low_idle(int pwm_id, int enable)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/low_idle", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", enable);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// 设置组模式
int anj_virdev_motor_set_group_join(int pwm_id, int enable)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/join", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d %d", pwm_id, enable);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


// 设置组周期
int anj_virdev_motor_set_group_period(int pwm_id, int period)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/pwm%d/period", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID, pwm_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", period);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// 设置组开始时间
int anj_virdev_motor_set_group_shift(int pwm_id, int shift)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/pwm%d/shift", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID, pwm_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", shift);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// 设置组结束时间
int anj_virdev_motor_set_group_duty(int pwm_id, int duty)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/pwm%d/duty", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID, pwm_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", duty);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


// 设置组极性
int anj_virdev_motor_set_group_polarity(int pwm_id, int polarity)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/pwm%d/polarity", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID, pwm_id);

    char buf[32] = {0};
    if (polarity == 1)
    {
        snprintf(buf, sizeof(buf), "normal");
    }
    else
    {
        snprintf(buf, sizeof(buf), "inversed");
    }

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_virdev_motor_set_group_round(int group_id, int step)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/round", VIRDEV_MOTOR_SYSFS_PATH, group_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", step);

    int fd = open(path, O_WRONLY);
    if (fd == 0)
    {
        __ERR("open vir dev:%s failed, but ignore!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_virdev_motor_set_group_enable(int group_id, int enable)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/g_enable", VIRDEV_MOTOR_SYSFS_PATH, group_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", enable);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed, but ignore!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_virdev_motor_group_round_clear(int group_id)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/round_cnt", VIRDEV_MOTOR_SYSFS_PATH, group_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", 0);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed, but ignore!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s %s failed!\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_virdev_motor_set_group_stop(int group_id, int stop_flag)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), "%s/group%d/stop", VIRDEV_MOTOR_SYSFS_PATH, group_id);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", stop_flag);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open vir dev:%s failed, but ignore!\n", path);
        return -1;
    }

    if (write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("write to vir dev:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    anj_virdev_motor_group_round_clear(group_id);
    return 0;
}

/******************************************************************************
@函数名称 : int anj_virdev_motor_duty_cycle_get()
@功能描述 : 获取PWM的占空比
@输入参数 : pwm_num:pwm接口 dir:电机转动的方向
@输出参数 : begin:开始拉高的时间 end:结束拉高的时间
@返回值： 成功返回0，失败返回-1
******************************************************************************/
static int anj_virdev_motor_duty_cycle_get(int pwm_num, int dir, int period, int *shift, int *duty)
{
    switch (pwm_num)
    {
    case MOTO_PWM_ID0:
    {
        if (dir == MOTOR_DIRECTION_FORWARD)
        {
            *shift = period * 2 / 8;
            *duty = period * 7 / 8;
        }
        else
        {
            *shift = period * 0 / 8;
            *duty = period * 5 / 8;
        }
        break;
    }

    case MOTO_PWM_ID1:
    {
        if (dir == MOTOR_DIRECTION_FORWARD)
        {
            *shift = period * 1 / 8;
            *duty = period * 4 / 8;
        }
        else
        {
            *shift = period * 3 / 8;
            *duty = period * 6 / 8;
        }
        break;
    }

    case MOTO_PWM_ID2:
    {
        if (dir == MOTOR_DIRECTION_FORWARD)
        {
            *shift = period * 3 / 8;
            *duty = period * 6 / 8;
        }
        else
        {
            *shift = period * 1 / 8;
            *duty = period * 4 / 8;
        }
        break;
    }

    case MOTO_PWM_ID3:
    {
        if (dir == MOTOR_DIRECTION_FORWARD)
        {
            *shift = period * 0 / 8;
            *duty = period * 5 / 8;
        }
        else
        {
            *shift = period * 2 / 8;
            *duty = period * 7 / 8;
        }
        break;
    }

    default:
    {
        __INFO("invalid pwm num :%d\n", pwm_num);
        return -1;
    }
    }
    return 0;
}

static void anj_virdev_motor_pwm_init(int pwm_num, int period, int dir)
{
    int duty = 0;
    int shift = 0;
    anj_virdev_motor_set_group_period(pwm_num, period);
    anj_virdev_motor_duty_cycle_get(pwm_num, dir, period, &shift, &duty);
    anj_virdev_motor_set_group_shift(pwm_num, shift);
    anj_virdev_motor_set_group_duty(pwm_num, duty);

    if (pwm_num == MOTO_PWM_ID3 || pwm_num == MOTO_PWM_ID0)
    {
        anj_virdev_motor_set_group_polarity(pwm_num, 1);
    }
    else
    {
        anj_virdev_motor_set_group_polarity(pwm_num, 0);
    }
}

void anj_virdev_motor_init()
{
    anj_virdev_motor_pwm_init(MOTO_PWM_ID0, DEFAULT_PERIOD, 0);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID1, DEFAULT_PERIOD, 0);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID2, DEFAULT_PERIOD, 0);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID3, DEFAULT_PERIOD, 0);
    anj_virdev_motor_select_init();
}

void anj_virdev_motor_uninit()
{
    anj_virdev_motor_select_uninit();
}

void anj_virdev_motor_up(int step, int period)
{
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 0);
    anj_virdev_motor_select_v();
    anj_virdev_motor_pwm_init(MOTO_PWM_ID0, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID1, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID2, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID3, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_group_round_clear(MOTO_PWM_GROUP_ID);
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 1);
    anj_virdev_motor_set_group_round(MOTO_PWM_GROUP_ID, step);
    s_stMotorRunParam.bRun = 1;
    s_stMotorRunParam.Step = step;
    s_stMotorRunParam.Period = period;
}

void anj_virdev_motor_down(int step, int period)
{
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 0);
    anj_virdev_motor_select_v();
    anj_virdev_motor_pwm_init(MOTO_PWM_ID0, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID1, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID2, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID3, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_group_round_clear(MOTO_PWM_GROUP_ID);
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 1);
    anj_virdev_motor_set_group_round(MOTO_PWM_GROUP_ID, step);
    s_stMotorRunParam.bRun = 1;
    s_stMotorRunParam.Step = step;
    s_stMotorRunParam.Period = period;
}

void anj_virdev_motor_left(int step, int period)
{
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 0);
    anj_virdev_motor_select_h();
    anj_virdev_motor_pwm_init(MOTO_PWM_ID0, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID1, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID2, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID3, period, MOTOR_DIRECTION_BACKWARD);
    anj_virdev_motor_group_round_clear(MOTO_PWM_GROUP_ID);
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 1);
    anj_virdev_motor_set_group_round(MOTO_PWM_GROUP_ID, step);
    s_stMotorRunParam.bRun = 1;
    s_stMotorRunParam.Step = step;
    s_stMotorRunParam.Period = period;
}

void anj_virdev_motor_right(int step, int period)
{
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 0);
    anj_virdev_motor_select_h();
    anj_virdev_motor_pwm_init(MOTO_PWM_ID0, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID1, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID2, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_pwm_init(MOTO_PWM_ID3, period, MOTOR_DIRECTION_FORWARD);
    anj_virdev_motor_group_round_clear(MOTO_PWM_GROUP_ID);
    anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 1);
    anj_virdev_motor_set_group_round(MOTO_PWM_GROUP_ID, step);
    s_stMotorRunParam.bRun = 1;
    s_stMotorRunParam.Step = step;
    s_stMotorRunParam.Period = period;
}

int anj_virdev_motor_get_group_round()
{
    char path[256] = {0};
    snprintf(path, sizeof(path), "%s/group%d/round_cnt", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID);

    /*获取电机转动次数*/
    char round_info[64] = {0};
    snprintf(path, sizeof(path), "cat %s/group%d/round_cnt", VIRDEV_MOTOR_SYSFS_PATH, MOTO_PWM_GROUP_ID);
    FILE *fp = popen(path, "r");
    if (fp == NULL)
    {
        __ERR("popen return NULL buf:%s\n", path);
        return -1;
    }
    else
    {
        fread(round_info, 1, sizeof(round_info) - 1, fp);
        pclose(fp);
    }

    int round = atoi(round_info);
    return round;
}

int anj_virdev_motor_stop()
{
    int iRunAlready = 0; /*此时点击已经跑了的步数*/
    int iRunStep = 0;    /*电机已经跑的步数*/
    int iRunTotal = 0;   /*电机实际停止时总共要跑的步数 为了防止步数获取的不正确*/
    int iRemainStep = 0; /*电机剩余要跑的步数*/
    int bOverTime = 0;   /*超时时间*/

    if (s_stMotorRunParam.bRun)
    {
        iRunAlready = anj_virdev_motor_get_group_round();
        iRunTotal = iRunAlready + MOTOR_WAIT_STOP_STEP;

        if (iRunTotal < s_stMotorRunParam.Step)
        {
            anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 1);
            anj_virdev_motor_set_group_round(MOTO_PWM_GROUP_ID, MOTOR_WAIT_STOP_STEP);
        }
        else
        {
            iRunTotal = s_stMotorRunParam.Step;
        }

        // N:2026.03.05 385平台最新patch，在上一次的round没走完时再设置round，此时的round_cnt会从0开始计数
        while (iRunTotal > (iRunStep + iRunAlready))
        {
            bOverTime++;
            anj_mw_rsleep(10 * 1000);
            iRunStep = anj_virdev_motor_get_group_round();

            /*防止云台长期阻塞,一分钟就跳出*/
            if (bOverTime >= (MOTOR_OVER_TIME / (MOTOR_WAIT_TIME / 10)))
            {
                __ERR("######PTZ Over Time!! %d\n", bOverTime);
                break;
            }
        }
        iRemainStep = s_stMotorRunParam.Step - iRunStep;
        anj_virdev_motor_set_group_enable(MOTO_PWM_GROUP_ID, 0);
        iRunStep = anj_virdev_motor_get_group_round();
        iRemainStep = s_stMotorRunParam.Step - iRunStep;
        anj_virdev_motor_set_group_stop(MOTO_PWM_GROUP_ID, 1);
    }
    s_stMotorRunParam.bRun = 0;
    s_stMotorRunParam.Step = 0;
    s_stMotorRunParam.Period = 0;

    if (iRemainStep < 0)
    {
        iRemainStep = 0;
    }

    anj_virdev_motor_set_group_stop(MOTO_PWM_GROUP_ID, 0);
    return iRemainStep;
}
