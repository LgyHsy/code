
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "anj_mw_log.h"
#include "anj_mw_pwm.h"
#include "anj_mw_file.h"

int anj_mw_pwm_export(int chip_num, int pwm_num)
{
    if (chip_num < 0 || pwm_num < 0)
    {
        __ERR("error chip_num:%d or pwm_num:%d\n", chip_num, pwm_num);
        return -1;
    }

    char export_path[PWM_MAX_PATH_LEN] = {0};
    snprintf(export_path, sizeof(export_path), PWM_PATH_PREFIX "export", chip_num);

    char buf[16] = {0};
    snprintf(buf, sizeof(buf), "%d", pwm_num);

    int fd = open(export_path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open pwm path %s failed\n", export_path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to pwm path %s buf:%s failed!\n", export_path, buf);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_unexport(int chip_num, int pwm_num)
{
    if (chip_num < 0 || pwm_num < 0)
    {
        __ERR("error chip_num:%d or pwm_num:%d\n", chip_num, pwm_num);
        return -1;
    }

    char unexport_path[PWM_MAX_PATH_LEN] = {0};
    snprintf(unexport_path, sizeof(unexport_path), PWM_PATH_PREFIX "/unexport", chip_num);

    char buf[16] = {0};
    snprintf(buf, sizeof(buf), "%d", pwm_num);

    int fd = open(unexport_path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open pwm path %s failed!\n", unexport_path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to pwm path %s failed\n", unexport_path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_write_attr(int chip_num, int pwm_num, const char *attr, const char *value)
{
    char path[PWM_MAX_PATH_LEN] = {0};
    snprintf(path, sizeof(path), PWM_PATH_PREFIX "pwm%d/%s", chip_num, pwm_num, attr);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open pwm path %s failed\n", path);
        return -1;
    }

    int len = safe_write(fd, (void *)value, strlen(value));
    if (len == -1)
    {
        __ERR("safe_write to pwm path %s failed\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

// 读取PWM属性(通用函数)
static int anj_mw_pwm_read_attr(int chip_num, int pwm_num, const char *attr, char *buf, size_t buf_size)
{
    char path[PWM_MAX_PATH_LEN] = {0};
    snprintf(path, sizeof(path), PWM_PATH_PREFIX "pwm%d/%s", chip_num, pwm_num, attr);
    int fd = open(path, O_RDONLY);
    if (fd == -1)
    {
        __ERR("open pwm path %s failed\n", path);
        return -1;
    }
    int len = safe_read(fd, buf, buf_size - 1);
    if (len == -1)
    {
        __ERR("safe_read from pwm path  %s failed\n", path);
        close(fd);
        return -1;
    }

    buf[len] = '\0';
    close(fd);
    return 0;
}

int anj_mw_pwm_set_period(int chip_num, int pwm_num, int period_ns)
{
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", period_ns);
    return anj_mw_pwm_write_attr(chip_num, pwm_num, "period", buf);
}

int anj_mw_pwm_set_duty_cycle(int chip_num, int pwm_num, int duty_ns)
{
    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", duty_ns);
    return anj_mw_pwm_write_attr(chip_num, pwm_num, "duty_cycle", buf);
}

// 设置PWM极性(normal或inversed)
int anj_mw_pwm_set_polarity(int chip_num, int pwm_num, const char *polarity)
{
    if (strcmp(polarity, "normal") != 0 && strcmp(polarity, "inversed") != 0)
    {
        __ERR("invalid pwm polarity: %s (must be 'normal' or 'inversed')\n", polarity);
        return -1;
    }

    return anj_mw_pwm_write_attr(chip_num, pwm_num, "polarity", polarity);
}

int anj_mw_pwm_set_enable(int chip_num, int pwm_num, int enable)
{
    return anj_mw_pwm_write_attr(chip_num, pwm_num, "enable", enable ? "1" : "0");
}

int anj_mw_pwm_get_period(int chip_num, int pwm_num, int *period_ns)
{
    char buf[32] = {0};
    if (anj_mw_pwm_read_attr(chip_num, pwm_num, "period", buf, sizeof(buf)) != 0)
    {
        return -1;
    }

    *period_ns = (int)strtoul(buf, NULL, 10);
    return 0;
}

int anj_mw_pwm_get_duty_cycle(int chip_num, int pwm_num, int *duty_ns)
{
    char buf[32] = {0};
    if (anj_mw_pwm_read_attr(chip_num, pwm_num, "duty_cycle", buf, sizeof(buf)) != 0)
    {
        return -1;
    }

    *duty_ns = (int)strtoul(buf, NULL, 10);
    return 0;
}

int anj_mw_pwm_get_polarity(int chip_num, int pwm_num, char *polarity, size_t buf_size)
{
    return anj_mw_pwm_read_attr(chip_num, pwm_num, "polarity", polarity, buf_size);
}

int anj_mw_pwm_get_enable(int chip_num, int pwm_num, int *enable)
{
    char buf[8] = {0};
    if (anj_mw_pwm_read_attr(chip_num, pwm_num, "enable", buf, sizeof(buf)) != 0)
    {
        return -1;
    }

    *enable = atoi(buf);
    return 0;
}

int anj_mw_pwm_set_group_period(int pwm_id, int group, int period)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "g_period", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d %d", pwm_id, period);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_shift(int pwm_id, int group, int shift)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "g_shift", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d %d", pwm_id, shift);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_duty(int pwm_id, int group, int duty)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "g_duty", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d %d", pwm_id, duty);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_polarity(int pwm_id, int group, int polarity)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "g_polarity", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d %d", pwm_id, polarity);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_low_idle(int enable, int group)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "low_idle", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", enable);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed, but ignore!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_enable(int enable, int group)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "g_enable", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", enable);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed, but ignore!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_set_group_round(int step, int group)
{
    char path[64] = {0};
    snprintf(path, sizeof(path), PWM_GROUP_PATH "round", group);

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%d", step);

    int fd = open(path, O_WRONLY);
    if (fd == -1)
    {
        __ERR("open sstar pwm group:%s failed, but ignore!\n", path);
        return -1;
    }

    if (safe_write(fd, buf, strlen(buf)) == -1)
    {
        __ERR("safe_write to sstar pwm group:%s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int anj_mw_pwm_gpio_ctrl(int cmd)
{
    char path[PWM_MAX_PATH_LEN] = {0};
    snprintf(path, sizeof(path), "/dev/gpiopwm");

    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd == -1)
    {
        __ERR("open pwm path %s failed: %s\n", path);
        return -1;
    }

    int ret = ioctl(fd, cmd, 0);
    if (ret == -1)
    {
        __ERR("ioctl pwm path %s failed!\n", path);
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}
