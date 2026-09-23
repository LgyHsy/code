#ifndef __ANJ_MW_HWCTRL_H__
#define __ANJ_WM_HWCTRL_H__

#ifdef __cplusplus
extern "C"
{
#endif


/*
    红白PWM灯亮度设置 ircut设置 光敏获取
*/
int anj_mw_hwctrl_photo_sensor_get();
int anj_mw_hwctrl_photo_sensor_int();
int anj_mw_hwctrl_photo_sensor_uninit();

int anj_mw_hwctrl_pwm_rlight_set(int camera, int value);
int anj_mw_hwctrl_pwm_rlight_get(int camera);
int anj_mw_hwctrl_pwm_wlight_set(int camera, int value);
int anj_mw_hwctrl_pwm_wlight_get(int camera);
int anj_mw_hwctrl_pwm_light_init(int camera, int period, int duty_cycle);
int anj_mw_hwctrl_pwm_light_uninit(int camera);

void anj_mw_hwctrl_wlight_period_flicker(int period_ms, int duration_ms);

/*
    红蓝报警led操作：打开、关闭、延时关闭、周期闪烁
*/
int anj_mw_hwctrl_alarmled_open();
int anj_mw_hwctrl_alarmled_close();
void anj_mw_hwctrl_alarmled_toggle_delay(int delayMs);
int anj_mw_hwctrl_alarmled_flicker(int periodMs);
int anj_mw_hwctrl_alarmled_init();
int anj_mw_hwctrl_alarmled_uninit();

int anj_mw_hwctrl_switch_simcard_status(int status);
int anj_mw_hwctrl_usb_status(int status);
int anj_mw_hwctrl_gyro_power_status(int status);

/*
    AOV三个引脚：4gResume(输出)，4gStat(输入)，charge(输入)
*/
int anj_mw_hwctrl_aov_4g_resume_get();
int anj_mw_hwctrl_aov_4g_resume_status(int status);
int anj_mw_hwctrl_aov_4g_resume_init();
int anj_mw_hwctrl_aov_4g_resume_uninit();

int anj_mw_hwctrl_aov_4g_stat_get();
int anj_mw_hwctrl_aov_4g_stat_init();
int anj_mw_hwctrl_aov_4g_stat_uninit();

int anj_mw_hwctrl_aov_charge_get();
int anj_mw_hwctrl_aov_charge_init();
int anj_mw_hwctrl_aov_charge_uninit();


/*
    复位键操作：初始化、反初始化
*/
int anj_mw_hwctrl_reset_get();
int anj_mw_hwctrl_reset_init();
int anj_mw_hwctrl_reset_uninit();

int anj_mw_hwctrl_switch_ao_open();
int anj_mw_hwctrl_switch_ao_close();
int anj_mw_hwctrl_switch_ao_init();
int anj_mw_hwctrl_switch_ao_uninit();

/*
    ircut操作
*/
int anj_mw_hwctrl_ircut_set_day();
int anj_mw_hwctrl_ircut_set_night();
int anj_mw_hwctrl_ircut_init();
int anj_mw_hwctrl_ircut_uninit();

int anj_mw_hwctrl_led_on();
int anj_mw_hwctrl_led_close();
int anj_mw_hwctrl_led_init();
int anj_mw_hwctrl_led_uninit();


int anj_mw_hwctrl_alarmout_get_remain_time(int chn);
void anj_mw_hwctrl_alarmout_toggle_delay(int chn, int state, int delayms);
void anj_mw_hwctrl_alarmout_modify_end_time(int chn, int delayms);
int anj_mw_hwctrl_alarmout_get_port(int chn);
int anj_mw_hwctrl_alarmout_init(int chn, const char *pStatus);
int anj_mw_hwctrl_alarmout_uninit(int chn);
/*
    获取可用的alarmout gpio端口的值
*/
int anj_mw_hwctrl_alarmout_port_count_get();
int anj_mw_hwctrl_alarmout_usable_chn_status_get();
int anj_mw_hwctrl_alarmout_usable_chn_status_set(int status);
int anj_mw_hwctrl_alarmout_chn_status_get(int chn);
int anj_mw_hwctrl_alarmout_chn_status_set(int chn, int status);

/*
    alarm in
*/
int anj_mw_hwctrl_alarmin_init(void);
int anj_mw_hwctrl_alarmin_uninit(void);
int anj_mw_hwctrl_alarmin_port_count_get(void);
int anj_mw_hwctrl_alarmin_chn_status_get(int chn);


/*
    某些还未使用宏定义的gpio操作：停止闪烁、延时翻转、周期闪烁
*/
int anj_mw_hwctrl_stop_filcker(int port, int value);
int anj_mw_hwctrl_toggle_delay(int port, int value, int delayMs);
int anj_mw_hwctrl_flicker(int port, int value, int periodMs);

/*
    gpio 初始化
*/
void anj_mw_hwctrl_init();
void anj_mw_hwctrl_uninit();


// 设置PWM周期(ns)
int anj_mw_hwctrl_pwm_set_period(int chipnum, int chnnum, int period_ns);

// 设置PWM占空比(ns)
int anj_mw_hwctrl_pwm_set_duty_cycle(int chipnum, int chnnum, int duty_ns);

// 设置PWM极性(normal或inversed)
int anj_mw_hwctrl_pwm_set_polarity(int chipnum, int chnnum, const char *polarity);

int anj_mw_hwctrl_pwm_set_enable(int chipnum, int chnnum, int enable);

/*
    pwm 初始化
*/
int anj_mw_hwctrl_pwm_init(int chipnum, int chnnum, int period, int dutycycle);
int anj_mw_hwctrl_pwm_uninit(int chipnum, int chnnum);


/*
    "/sys/class/sstar/pwm/group1/" 操作接口，用于马达控制
*/
int anj_mw_hwctrl_pwm_set_grp_period(int pwm_id, int group, int period);
int anj_mw_hwctrl_pwm_set_grp_shift(int pwm_id, int group, int shift);
int anj_mw_hwctrl_pwm_set_grp_duty(int pwm_id, int group, int duty);
int anj_mw_hwctrl_pwm_set_grp_polarity(int pwm_id, int group, int polarity);

int anj_mw_hwctrl_pwm_set_grp_low_idle(int enable, int group);
int anj_mw_hwctrl_pwm_set_grp_enable(int enable, int group);
int anj_mw_hwctrl_pwm_set_grp_round(int step, int group);

void anj_mw_hwctrl_motor_init();
void anj_mw_hwctrl_motor_uninit();
void anj_mw_hwctrl_motor_up(int step, int period);
void anj_mw_hwctrl_motor_down(int step, int period);
void anj_mw_hwctrl_motor_left(int step, int period);
void anj_mw_hwctrl_motor_right(int step, int period);
int anj_mw_hwctrl_motor_remain_step();
int anj_mw_hwctrl_motor_stop();

int anj_mw_hwctrl_batteryadc_init();
int anj_mw_hwctrl_batteryadc_uninit();
int anj_mw_hwctrl_batteryadc_get();

int anj_mw_hwctrl_usb_init();
int anj_mw_hwctrl_usb_uninit();
int anj_mw_hwctrl_gyro_power_init();
int anj_mw_hwctrl_gyro_power_uninit();

#ifdef __cplusplus
}
#endif


#endif
