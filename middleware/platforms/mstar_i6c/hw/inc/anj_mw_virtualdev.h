#ifndef __ANJ_MW_VIRTUALDEV_H__
#define __ANJ_MW_VIRTUALDEV_H__

#define VIRDEV_MOTOR_SYSFS_PATH "/sys/devices/virtual/mstar/motor/"

// 设置组模式
int anj_virdev_motor_set_group_mode(int pwm_id, int mode);

// 设置组周期
int anj_virdev_motor_set_group_period(int pwm_id, int period);

// 设置组开始时间
int anj_virdev_motor_set_group_begin(int pwm_id, int begin);

// 设置组结束时间
int anj_virdev_motor_set_group_end(int pwm_id, int end);

// 设置组极性
int anj_virdev_motor_set_group_polarity(int pwm_id, int polarity);

int anj_virdev_motor_set_group_round(int group_id, int step);
int anj_virdev_motor_set_group_enable(int group_id, int enable);
int anj_virdev_motor_set_group_stop(int group_id, int stop_flag);

void anj_virdev_motor_init();
void anj_virdev_motor_uninit();
void anj_virdev_motor_up(int step, int period);
void anj_virdev_motor_down(int step, int period);
void anj_virdev_motor_left(int step, int period);
void anj_virdev_motor_right(int step, int period);
int anj_virdev_motor_get_group_round();
int anj_virdev_motor_stop();

#endif
