#ifndef __ANJ_MW_GPIODEV_H__
#define __ANJ_MW_GPIODEV_H__

#define ANJ_GPIO_EXPAND_DEV                     "/dev/expand_gpio"

#define EXPAND_GPIO_PORT_ALARMLED           5
#define EXPAND_GPIO_PORT_AUDIOOUT           1
#define EXPAND_GPIO_PORT_LED_MAIN           2
#define EXPAND_GPIO_PORT_LED_SUB            6

#define EXPAND_GPIO_TWO_LED_SEPARATE        1       // 两个LED扩展GPIO分开控制

#define EXPAND_GPIO_HIGH                1
#define EXPAND_GPIO_LOW                 0

#define EXPAND_GPIO_DEV_PIN00_LOW       0
#define EXPAND_GPIO_DEV_PIN00_HIGH      1

#define EXPAND_GPIO_DEV_PIN01_LOW       2
#define EXPAND_GPIO_DEV_PIN01_HIGH      3

#define EXPAND_GPIO_DEV_PIN02_LOW       4
#define EXPAND_GPIO_DEV_PIN02_HIGH      5

#define EXPAND_GPIO_DEV_PIN03_LOW       6
#define EXPAND_GPIO_DEV_PIN03_HIGH      7

#define EXPAND_GPIO_DEV_PIN04_LOW       8
#define EXPAND_GPIO_DEV_PIN04_HIGH      9

#define EXPAND_GPIO_DEV_PIN05_LOW       10
#define EXPAND_GPIO_DEV_PIN05_HIGH      11

#define EXPAND_GPIO_DEV_PIN06_LOW       12
#define EXPAND_GPIO_DEV_PIN06_HIGH      13

#define EXPAND_GPIO_DEV_PIN07_LOW       14
#define EXPAND_GPIO_DEV_PIN07_HIGH      15

#define EXPAND_GPIO_DEV_PIN10_LOW       16
#define EXPAND_GPIO_DEV_PIN10_HIGH      17

#define EXPAND_GPIO_DEV_PIN11_LOW       18
#define EXPAND_GPIO_DEV_PIN11_HIGH      19

#define EXPAND_GPIO_DEV_PIN12_LOW       20
#define EXPAND_GPIO_DEV_PIN12_HIGH      21

#define EXPAND_GPIO_DEV_PIN13_LOW       22
#define EXPAND_GPIO_DEV_PIN13_HIGH      23

#define EXPAND_GPIO_DEV_PIN14_LOW       24
#define EXPAND_GPIO_DEV_PIN14_HIGH      25

#define EXPAND_GPIO_DEV_PIN15_LOW       26
#define EXPAND_GPIO_DEV_PIN15_HIGH      27

#define EXPAND_GPIO_DEV_PIN16_LOW       28
#define EXPAND_GPIO_DEV_PIN16_HIGH      29

#define EXPAND_GPIO_DEV_PIN17_LOW       30
#define EXPAND_GPIO_DEV_PIN17_HIGH      31

#define EXPAND_GPIO_DEV_GROUP0_READ     32
#define EXPAND_GPIO_DEV_GROUP1_READ     33


int anj_gpio_expand_dev_port_value_macth(int port, int *low_value, int *high_value);

int anj_gpio_expand_dev_value_set(char value);

int anj_gpio_expand_dev_group1_value_get();

int anj_gpio_expand_dev_group2_value_get();


int anj_gpio_expand_switch_ao_ctrl(int value);

int anj_gpio_expand_alarm_led_ctrl(int value);

int anj_gpio_expand_switch_led_ctrl(int value);


int anj_gpio_expand_dev_init();

int anj_gpio_expand_dev_uninit();


#endif
