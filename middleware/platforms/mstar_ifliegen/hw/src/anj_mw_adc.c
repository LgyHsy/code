#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <asm/ioctl.h>

#include "anj_mw_log.h"
#include "anj_mw_adc.h"

static int s_BatteryAdcFd = -1;

int anj_mw_adc_battery_init()
{
    if (s_BatteryAdcFd > 0)
    {
        __ERR("battery adc fd:%d already exist so close!\n", s_BatteryAdcFd);
        close(s_BatteryAdcFd);
        s_BatteryAdcFd = -1;
    }

    s_BatteryAdcFd = open(ANJ_BAT_ADC_DEV, O_RDWR);
    if (s_BatteryAdcFd <= 0)
    {
        __ERR("battery adc dev open failed\n");
        return -1;
    }

    ioctl(s_BatteryAdcFd, MS_SAR_INIT, 0);
    __INFO("battery adc dev open successful! fd:%d\n", s_BatteryAdcFd);
    return 0;
}

int anj_mw_adc_battery_uninit()
{
    if (s_BatteryAdcFd > 0)
    {
        close(s_BatteryAdcFd);
        s_BatteryAdcFd = -1;
    }

    return 0;
}

int anj_mw_adc_battery_get()
{
    int value = 0;
    if (s_BatteryAdcFd <= 0)
    {
        return value;
    }

    unsigned short adc_value;
    ioctl(s_BatteryAdcFd, MS_SAR_SET_CHANNEL_READ_VALUE, &adc_value);
    value = adc_value;

    return value;
}