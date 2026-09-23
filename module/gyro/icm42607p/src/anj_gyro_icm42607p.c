#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "project_option.h"
#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_hwctrl.h"

#include "anj_gyro_provider.h"
#include "icm42607p.h"

static int anj_gyro_icm42607p_init(void)
{
    char i2c_dev[24];

    if (ANJ_GPIO_PORT_GYRO_POWER <= 0 || ICM42607P_I2C_BUS_ID < 0)
    {
        __ERR("gyro power gpio or i2c bus not supported\n");
        return -1;
    }

    anj_mw_hwctrl_gyro_power_status(1);
    usleep(ICM42607P_POWER_ON_DELAY_MS * 1000);

    snprintf(i2c_dev, sizeof(i2c_dev), "/dev/i2c-%d", ICM42607P_I2C_BUS_ID);

    if (icm42607p_open(i2c_dev, ICM42607P_I2C_ADDR) != 0)
    {
        __ERR("gyro icm42607p open failed on addr 0x%02x\n", ICM42607P_I2C_ADDR);
        return 1;
    }

    if (icm42607p_init() != 0)
    {
        __ERR("gyro icm42607p init failed on addr 0x%02x\n", ICM42607P_I2C_ADDR);
        icm42607p_close();
        return 1;
    }

    __INFO("gyro icm42607p ready on addr 0x%02x\n", ICM42607P_I2C_ADDR);
    return 0;
}

static int anj_gyro_icm42607p_uninit(void)
{
    icm42607p_close();
    anj_mw_hwctrl_gyro_power_status(0);
    return 0;
}

static int anj_gyro_icm42607p_read(anj_gyro_data_t *out)
{
    icm42607p_vec_t accel;
    icm42607p_vec_t gyro;
    float temp = 0.0f;

    if (out == NULL)
    {
        return -1;
    }

    if (icm42607p_read_all(&accel, &gyro, &temp) != 0)
    {
        return -1;
    }

    out->ready = 1;
    out->accel_x = accel.x;
    out->accel_y = accel.y;
    out->accel_z = accel.z;
    out->gyro_x = gyro.x;
    out->gyro_y = gyro.y;
    out->gyro_z = gyro.z;
    out->temp = temp;
    return 0;
}

static const anj_gyro_provider_ops s_stGyroIcm42607pProviderOps = {
    .provider_name = "icm42607p",
    .provider_priority = 100,
    .sample_interval_ms = ICM42607P_SAMPLE_INTERVAL_MS,
    .init = anj_gyro_icm42607p_init,
    .uninit = anj_gyro_icm42607p_uninit,
    .read = anj_gyro_icm42607p_read,
};

ANJ_LINK_KEEP(anj_keep_gyro_icm42607p_provider);

__attribute__((constructor)) static void anj_gyro_icm42607p_provider_register(void)
{
    anj_gyro_provider_register(&s_stGyroIcm42607pProviderOps);
}

__attribute__((destructor)) static void anj_gyro_icm42607p_provider_unregister(void)
{
    anj_gyro_provider_unregister(&s_stGyroIcm42607pProviderOps);
}
