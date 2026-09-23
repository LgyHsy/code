#ifndef _ANJ_MW_GPIO_H_
#define _ANJ_MW_GPIO_H_


#ifdef __cplusplus
extern "C"
{
#endif


typedef enum
{
    E_GPIO_ALARM_CHN_NONE = 0,
    E_GPIO_ALARM_CHN1 = 1,
    E_GPIO_ALARM_CHN2 = 2,
    E_GPIO_ALARM_CHN3 = 3,
    E_GPIO_ALARM_CHN4 = 4,
    E_GPIO_ALARM_CHN_MAX = 5
} E_GPIO_ALARM_CHN_e;

#define GPIO_DIREC_IN                         (0) 
#define GPIO_DIREC_OUT                        (1) 
#define GPIO_VALUE_LOW                        (0)
#define GPIO_VALUE_HIGH                       (1)
#define ANJ_GPIO_LEVEL_STR_HIGH                 "HIGH"
#define ANJ_GPIO_LEVEL_STR_LOW                  "LOW"

int anj_gpio_read_port_value(int port);
int anj_gpio_write_port_value(int port, int value);

int anj_gpio_write_port_direc(int port, int direc);
int anj_gpio_read_port_direc(int port);

int anj_gpio_write_port_export(int port);
int anj_gpio_write_port_unexport(int port);


#ifdef __cplusplus
}
#endif

#endif