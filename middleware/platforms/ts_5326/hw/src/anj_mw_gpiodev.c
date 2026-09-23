#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_gpiodev.h"

static int s_GpioExpandDevFd = -1;


int anj_gpio_expand_dev_port_value_macth(int port, int *low_value, int *high_value)
{
    switch(port)
    {
    case 0:
        *low_value = EXPAND_GPIO_DEV_PIN00_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN00_HIGH;
    break;
    case 1:
        *low_value = EXPAND_GPIO_DEV_PIN01_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN01_HIGH;
    break;
    case 2:
        *low_value = EXPAND_GPIO_DEV_PIN02_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN02_HIGH;
    break;
    case 3:
        *low_value = EXPAND_GPIO_DEV_PIN03_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN03_HIGH;
    break;
    case 4:
        *low_value = EXPAND_GPIO_DEV_PIN04_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN04_HIGH;
    break;
    case 5:
        *low_value = EXPAND_GPIO_DEV_PIN05_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN05_HIGH;
    break;
    case 6:
        *low_value = EXPAND_GPIO_DEV_PIN06_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN06_HIGH;
    break;
    case 7:
        *low_value = EXPAND_GPIO_DEV_PIN07_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN07_HIGH;
    break;
    case 10:
        *low_value = EXPAND_GPIO_DEV_PIN10_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN10_HIGH;
    break;
    case 11:
        *low_value = EXPAND_GPIO_DEV_PIN11_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN11_HIGH;
    break;
    case 12:
        *low_value = EXPAND_GPIO_DEV_PIN12_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN12_HIGH;
    break;
    case 13:
        *low_value = EXPAND_GPIO_DEV_PIN13_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN13_HIGH;
    break;
    case 14:
        *low_value = EXPAND_GPIO_DEV_PIN14_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN14_HIGH;
    break;
    case 15:
        *low_value = EXPAND_GPIO_DEV_PIN15_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN15_HIGH;
    break;
    case 16:
        *low_value = EXPAND_GPIO_DEV_PIN16_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN16_HIGH;
    break;
    case 17:
        *low_value = EXPAND_GPIO_DEV_PIN17_LOW;
        *high_value = EXPAND_GPIO_DEV_PIN17_HIGH;
    break;
    default:
        __ERR("expand gpio port:%d don't support!\n", port);
    break;

    }

    return 0;
}

int anj_gpio_expand_dev_value_set(char value)
{
    if (s_GpioExpandDevFd <= 0)
    {
        return -1;
    }

    if (value < 0)
    {
        return -1;
    }

    int iRet = safe_write(s_GpioExpandDevFd, &value, 1);
    if (iRet)
    {
        __ERR("gpio expand dev write value:%c failed\n", value);
    }
    return iRet;
}

int anj_gpio_expand_dev_group0_value_get()
{
    if (s_GpioExpandDevFd <= 0)
    {
        return -1;
    }

    int value = 0;
    safe_read(s_GpioExpandDevFd, &value, 1);

    return value;
}

int anj_gpio_expand_dev_group1_value_get()
{
    if (s_GpioExpandDevFd <= 0)
    {
        return -1;
    }

    int value = 0;
    safe_read(s_GpioExpandDevFd, &value, 2);

    return value;
}

int anj_gpio_expand_switch_ao_ctrl(int value)
{
    int iRet = 0;
    int cmd_value = (value == EXPAND_GPIO_HIGH) ? EXPAND_GPIO_DEV_PIN01_HIGH : EXPAND_GPIO_DEV_PIN01_LOW;
    iRet = anj_gpio_expand_dev_value_set(cmd_value);
    return iRet;
}

int anj_gpio_expand_alarm_led_ctrl(int value)
{
    int iRet = 0;
    int cmd_value = (value == EXPAND_GPIO_HIGH) ? EXPAND_GPIO_DEV_PIN05_HIGH : EXPAND_GPIO_DEV_PIN05_LOW;
    iRet = anj_gpio_expand_dev_value_set(cmd_value);
    return iRet;
}

int anj_gpio_expand_switch_led_ctrl(int value)
{
    int iRet = 0;
    int cmd_value = 0;

    cmd_value = (value == EXPAND_GPIO_HIGH) ? EXPAND_GPIO_DEV_PIN02_HIGH : EXPAND_GPIO_DEV_PIN02_LOW;
    iRet = anj_gpio_expand_dev_value_set(cmd_value);

    if (EXPAND_GPIO_TWO_LED_SEPARATE > 0)
    {
        cmd_value = (value == EXPAND_GPIO_HIGH) ? EXPAND_GPIO_DEV_PIN06_HIGH : EXPAND_GPIO_DEV_PIN06_LOW;
        iRet = anj_gpio_expand_dev_value_set(cmd_value);
    }

    return iRet;
}

int anj_gpio_expand_dev_init()
{
    if (s_GpioExpandDevFd > 0)
    {
        return 0;
    }

    s_GpioExpandDevFd = open(ANJ_GPIO_EXPAND_DEV, O_RDWR);
    if (s_GpioExpandDevFd <= 0)
    {
        __ERR("gpio expand dev open failed\n");
        return -1;
    }

    anj_gpio_expand_dev_value_set(EXPAND_GPIO_DEV_PIN04_HIGH);

    return 0;
}

int anj_gpio_expand_dev_uninit()
{
    if (s_GpioExpandDevFd > 0)
    {
        close(s_GpioExpandDevFd);
        s_GpioExpandDevFd = -1;
    }

    return 0;
}


