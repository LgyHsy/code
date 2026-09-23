#ifndef __ANJ_MW_PWM_H__
#define __ANJ_WM_PWM_H__

#define PWM_PATH_PREFIX                     "/sys/class/pwm/pwmchip%d/"
#define PWM_MAX_PATH_LEN                    64

#define PWM_GROUP_PATH               "/sys/class/sstar/pwm/group%d/"

int anj_mw_pwm_export(int chip_num, int pwm_num);
int anj_mw_pwm_unexport(int chip_num, int pwm_num);

int anj_mw_pwm_set_period(int chip_num, int pwm_num, int period_ns);
int anj_mw_pwm_set_duty_cycle(int chip_num, int pwm_num, int duty_ns);

// 设置PWM极性(normal或inversed)
int anj_mw_pwm_set_polarity(int chip_num, int pwm_num, const char *polarity);
int anj_mw_pwm_set_enable(int chip_num, int pwm_num, int enable);

int anj_mw_pwm_get_period(int chip_num, int pwm_num, int *period_ns);
int anj_mw_pwm_get_duty_cycle(int chip_num, int pwm_num, int *duty_ns);
int anj_mw_pwm_get_polarity(int chip_num, int pwm_num, char *polarity, size_t buf_size);
int anj_mw_pwm_get_enable(int chip_num, int pwm_num, int *enable);


// 设置组周期
int anj_mw_pwm_set_group_period(int pwm_id, int group, int period);

// 设置组偏移时间(begin)
int anj_mw_pwm_set_group_shift(int pwm_id, int group, int shift);

// 设置组占空比(end)
int anj_mw_pwm_set_group_duty(int pwm_id, int group, int duty);

// 设置组极性
int anj_mw_pwm_set_group_polarity(int pwm_id, int group, int polarity);

int anj_mw_pwm_set_group_low_idle(int enable, int group);

int anj_mw_pwm_set_group_enable(int enable, int group);

int anj_mw_pwm_set_group_round(int step, int group);

int anj_mw_pwm_gpio_ctrl(int cmd);


#endif
