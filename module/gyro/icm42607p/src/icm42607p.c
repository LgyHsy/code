#include <string.h>
#include <unistd.h>

#include "anj_mw_log.h"
#include "anj_mw_i2c_dev.h"
#include "icm42607p.h"

static anj_i2c_dev_t s_icm42607p_i2c = {.fd = -1};
static float s_accel_sensitivity = 4096.0f;
static float s_gyro_sensitivity = 32.8f;
static int s_accel_enabled = 0;
static int s_gyro_enabled = 0;

static int16_t icm42607p_to_int16(uint8_t h, uint8_t l)
{
    return (int16_t)(((uint16_t)h << 8) | l);
}

static int icm42607p_data_is_reset(int16_t raw)
{
    return raw == ICM42607P_DATA_RESET_RAW;
}

static int icm42607p_write_reg(uint8_t reg, uint8_t val)
{
    int ret = anj_i2c_dev_write_reg(&s_icm42607p_i2c, reg, &val, 1);

    if (ret == 0)
    {
        usleep(ICM42607P_REG_WRITE_DELAY_US);
    }

    return ret;
}

static int icm42607p_read_reg(uint8_t reg, uint8_t *val)
{
    if (val == NULL)
    {
        return -1;
    }

    return anj_i2c_dev_read_reg(&s_icm42607p_i2c, reg, val, 1);
}

static int icm42607p_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return anj_i2c_dev_read_reg(&s_icm42607p_i2c, reg, buf, len);
}

static int icm42607p_mclk_is_ready(uint8_t val)
{
    return (val & ICM42607P_MCLK_RDY_MASK) || (val & ICM42607P_MCLK_RDY_ALT_MASK);
}

static int icm42607p_wait_mclk_rdy(void)
{
    int i = 0;
    uint8_t val = 0;

    for (i = 0; i < ICM42607P_MCLK_RDY_TIMEOUT_MS; i++)
    {
        if (icm42607p_read_reg(ICM42607P_REG_MCLK_RDY, &val) != 0)
        {
            return -1;
        }

        if (icm42607p_mclk_is_ready(val))
        {
            return 0;
        }

        usleep(1000);
    }

    __ERR("icm42607p mclk not ready, reg=0x%02x\n", val);
    return -1;
}

static int icm42607p_wait_reset_done(void)
{
    int i = 0;
    uint8_t val = 0;

    for (i = 0; i < ICM42607P_RESET_DONE_TIMEOUT_MS; i++)
    {
        if (icm42607p_read_reg(ICM42607P_REG_INT_STATUS, &val) != 0)
        {
            return -1;
        }

        if (val & ICM42607P_INT_STATUS_RESET_DONE)
        {
            return 0;
        }

        usleep(1000);
    }

    __ERR("icm42607p reset done timeout, status=0x%02x\n", val);
    return -1;
}

static int icm42607p_burst_is_reset(const uint8_t *buf)
{
    if (s_accel_enabled &&
        icm42607p_data_is_reset(icm42607p_to_int16(buf[2], buf[3])))
    {
        return 1;
    }

    if (s_gyro_enabled &&
        icm42607p_data_is_reset(icm42607p_to_int16(buf[8], buf[9])))
    {
        return 1;
    }

    return 0;
}

static int icm42607p_soft_reset(void)
{
    if (icm42607p_write_reg(ICM42607P_REG_SIGNAL_PATH_RESET, ICM42607P_SOFT_RESET_DEVICE) != 0)
    {
        return -1;
    }

    usleep(1000);

    if (icm42607p_wait_reset_done() != 0)
    {
        return -1;
    }

    return icm42607p_wait_mclk_rdy();
}

static int icm42607p_i2c_bus_setup(void)
{
    uint8_t val = 0;

    if (icm42607p_read_reg(ICM42607P_REG_INTF_CONFIG1, &val) != 0)
    {
        return -1;
    }

    val &= (uint8_t)~0x0C;
    if (icm42607p_write_reg(ICM42607P_REG_INTF_CONFIG1, val) != 0)
    {
        return -1;
    }

    if (icm42607p_read_reg(ICM42607P_REG_DRIVE_CONFIG2, &val) != 0)
    {
        return -1;
    }

    val = (uint8_t)((val & (uint8_t)~0x38) | (0x01 << 3));
    if (icm42607p_write_reg(ICM42607P_REG_DRIVE_CONFIG2, val) != 0)
    {
        return -1;
    }

    if (icm42607p_read_reg(ICM42607P_REG_INTF_CONFIG0, &val) != 0)
    {
        return -1;
    }

    val |= 0x10;
    val = (uint8_t)((val & (uint8_t)~0x03) | ICM42607P_INTF_CONFIG0_SPI_DIS);

    return icm42607p_write_reg(ICM42607P_REG_INTF_CONFIG0, val);
}

static int icm42607p_set_intf_clksel(void)
{
    uint8_t val = 0;

    if (icm42607p_read_reg(ICM42607P_REG_INTF_CONFIG1, &val) != 0)
    {
        return -1;
    }

    val = (uint8_t)((val & (uint8_t)~0x03) | 0x01);

    return icm42607p_write_reg(ICM42607P_REG_INTF_CONFIG1, val);
}

int icm42607p_open(const char *dev_path, uint8_t addr)
{
    if (dev_path == NULL)
    {
        return -1;
    }

    return anj_i2c_dev_open(&s_icm42607p_i2c, dev_path, addr);
}

void icm42607p_close(void)
{
    anj_i2c_dev_close(&s_icm42607p_i2c);
    s_icm42607p_i2c.fd = -1;
    s_accel_enabled = 0;
    s_gyro_enabled = 0;
}

static int icm42607p_accel_config(uint8_t fs, uint8_t odr)
{
    switch (fs)
    {
    case ACCEL_FS_2G:
        s_accel_sensitivity = 16384.0f;
        break;
    case ACCEL_FS_4G:
        s_accel_sensitivity = 8192.0f;
        break;
    case ACCEL_FS_8G:
        s_accel_sensitivity = 4096.0f;
        break;
    default:
        s_accel_sensitivity = 2048.0f;
        break;
    }

    return icm42607p_write_reg(ICM42607P_REG_ACCEL_CONFIG0,
                            (uint8_t)(((fs & 0x03) << 5) | (odr & 0x0F)));
}

static int icm42607p_gyro_config(uint8_t fs, uint8_t odr)
{
    switch (fs)
    {
    case GYRO_FS_250DPS:
        s_gyro_sensitivity = 131.0f;
        break;
    case GYRO_FS_500DPS:
        s_gyro_sensitivity = 65.5f;
        break;
    case GYRO_FS_1000DPS:
        s_gyro_sensitivity = 32.8f;
        break;
    default:
        s_gyro_sensitivity = 16.4f;
        break;
    }

    return icm42607p_write_reg(ICM42607P_REG_GYRO_CONFIG0,
                            (uint8_t)(((fs & 0x03) << 5) | (odr & 0x0F)));
}

static int icm42607p_pwr_write_verify(uint8_t val, const char *step)
{
    uint8_t rb = 0;

    if (icm42607p_write_reg(ICM42607P_REG_PWR_MGMT0, val) != 0)
    {
        return -1;
    }

    usleep(ICM42607P_MODE_SETTLE_US);

    if (icm42607p_read_reg(ICM42607P_REG_PWR_MGMT0, &rb) != 0)
    {
        return -1;
    }

    __INFO("icm42607p pwr %s: wrote 0x%02x read 0x%02x\n", step, val, rb);

    if (rb != val)
    {
        __ERR("icm42607p pwr %s mismatch\n", step);
        return -1;
    }

    return 0;
}

static int icm42607p_sensor_config(void)
{
    uint8_t cfg1 = 0;

    if (icm42607p_pwr_write_verify(0x00, "off") != 0)
    {
        return -1;
    }

#if ICM42607P_PWR_MODE == ICM42607P_PWR_MODE_LN
    if (icm42607p_gyro_config(GYRO_FS_1000DPS, GYRO_ODR_100HZ) != 0)
    {
        return -1;
    }
#endif

    if (icm42607p_accel_config(ACCEL_FS_8G, ACCEL_ODR_100HZ) != 0)
    {
        return -1;
    }

#if ICM42607P_PWR_MODE == ICM42607P_PWR_MODE_LN
    if (icm42607p_write_reg(ICM42607P_REG_GYRO_CONFIG1, ICM42607P_FILTER_BW_25HZ) != 0)
    {
        return -1;
    }
#endif

    if (icm42607p_read_reg(ICM42607P_REG_ACCEL_CONFIG1, &cfg1) != 0)
    {
        return -1;
    }

    cfg1 = (uint8_t)((cfg1 & (uint8_t)~0x07) | ICM42607P_FILTER_BW_25HZ);

    return icm42607p_write_reg(ICM42607P_REG_ACCEL_CONFIG1, cfg1);
}

static int icm42607p_accel_data_is_valid(void)
{
    uint8_t buf[6];

    if (icm42607p_read_burst(ICM42607P_REG_ACCEL_DATA_X1, buf, sizeof(buf)) != 0)
    {
        return 0;
    }

    return !icm42607p_data_is_reset(icm42607p_to_int16(buf[0], buf[1]));
}

static int icm42607p_gyro_data_is_valid(void)
{
    uint8_t buf[6];

    if (icm42607p_read_burst(ICM42607P_REG_GYRO_DATA_X1, buf, sizeof(buf)) != 0)
    {
        return 0;
    }

    return !icm42607p_data_is_reset(icm42607p_to_int16(buf[0], buf[1]));
}

static int icm42607p_enable_accel_lp(void)
{
    uint8_t cfg1 = 0;

    if (icm42607p_pwr_write_verify(0x00, "off") != 0)
    {
        return -1;
    }

    if (icm42607p_accel_config(ACCEL_FS_8G, ACCEL_ODR_100HZ) != 0)
    {
        return -1;
    }

    if (icm42607p_read_reg(ICM42607P_REG_ACCEL_CONFIG1, &cfg1) != 0)
    {
        return -1;
    }

    cfg1 = (uint8_t)((cfg1 & (uint8_t)~0x07) | ICM42607P_FILTER_BW_25HZ);

    if (icm42607p_write_reg(ICM42607P_REG_ACCEL_CONFIG1, cfg1) != 0)
    {
        return -1;
    }

    if (icm42607p_pwr_write_verify(PWR_MGMT0_ACCEL_LP, "accel-lp") != 0)
    {
        return -1;
    }

    usleep(50000);
    return icm42607p_accel_data_is_valid() ? 0 : -1;
}

static int icm42607p_try_6axis_ln(void)
{
    if (icm42607p_pwr_write_verify(PWR_MGMT0_GYRO_STANDBY, "gyro-standby") != 0)
    {
        return -1;
    }

    usleep(ICM42607P_GYRO_STANDBY_MS * 1000);

    if (icm42607p_pwr_write_verify(PWR_MGMT0_GYRO_LN, "gyro-ln") != 0)
    {
        return -1;
    }

    usleep(ICM42607P_GYRO_STARTUP_MS * 1000);

    if (icm42607p_pwr_write_verify(PWR_MGMT0_SENSOR_LN, "6axis-ln") != 0)
    {
        return -1;
    }

    usleep(ICM42607P_GYRO_STARTUP_MS * 1000);
    return 0;
}

static int icm42607p_enable_sensors(void)
{
    s_accel_enabled = 0;
    s_gyro_enabled = 0;

#if ICM42607P_PWR_MODE == ICM42607P_PWR_MODE_LP
    if (icm42607p_pwr_write_verify(PWR_MGMT0_ACCEL_LP, "accel-lp") != 0)
    {
        return -1;
    }

    usleep(50000);

    if (!icm42607p_accel_data_is_valid())
    {
        __ERR("icm42607p accel-lp data invalid\n");
        return -1;
    }

    s_accel_enabled = 1;
    __INFO("icm42607p accel-lp mode (8g, 100Hz)\n");
    return 0;

#elif ICM42607P_PWR_MODE == ICM42607P_PWR_MODE_LN
    if (icm42607p_try_6axis_ln() == 0 && icm42607p_accel_data_is_valid())
    {
        s_accel_enabled = 1;
        s_gyro_enabled = icm42607p_gyro_data_is_valid();
        __INFO("icm42607p 6-axis ln, gyro=%d\n", s_gyro_enabled);
        return 0;
    }

    __WARN("icm42607p 6-axis ln unavailable, try accel-only modes\n");

    if (icm42607p_pwr_write_verify(0x00, "off") != 0)
    {
        return -1;
    }

    usleep(ICM42607P_MODE_SETTLE_US);

    if (icm42607p_pwr_write_verify(PWR_MGMT0_ACCEL_LN, "accel-ln") == 0)
    {
        usleep(20000);

        if (icm42607p_accel_data_is_valid())
        {
            s_accel_enabled = 1;
            __INFO("icm42607p accel-ln only (gyro unavailable)\n");
            return 0;
        }
    }

    if (icm42607p_enable_accel_lp() == 0)
    {
        s_accel_enabled = 1;
        __INFO("icm42607p accel-lp only (gyro unavailable)\n");
        return 0;
    }

    return -1;
#else
#error "ICM42607P_PWR_MODE must be ICM42607P_PWR_MODE_LP or ICM42607P_PWR_MODE_LN"
#endif
}

int icm42607p_whoami(uint8_t *id)
{
    if (id == NULL)
    {
        return -1;
    }

    return icm42607p_read_reg(ICM42607P_REG_WHO_AM_I, id);
}

int icm42607p_init(void)
{
    uint8_t id = 0;
    uint8_t pwr_rb = 0;
    uint8_t buf[14];

    if (icm42607p_wait_mclk_rdy() != 0)
    {
        return -1;
    }

    if (icm42607p_whoami(&id) != 0)
    {
        __ERR("icm42607p whoami failed\n");
        return -2;
    }

    if (id != ICM42607P_ID)
    {
        __ERR("icm42607p whoami mismatch: 0x%02x\n", id);
        return -3;
    }

    __INFO("icm42607p whoami ok: 0x%02x\n", id);

    if (icm42607p_soft_reset() != 0)
    {
        __ERR("icm42607p soft reset failed\n");
        return -4;
    }

    if (icm42607p_i2c_bus_setup() != 0)
    {
        __ERR("icm42607p i2c bus setup failed\n");
        return -5;
    }

    if (icm42607p_set_intf_clksel() != 0)
    {
        __ERR("icm42607p clksel setup failed\n");
        return -6;
    }

    if (icm42607p_sensor_config() != 0)
    {
        __ERR("icm42607p sensor config failed\n");
        return -7;
    }

    if (icm42607p_enable_sensors() != 0)
    {
        __ERR("icm42607p enable sensors failed\n");
        return -8;
    }

    if (icm42607p_read_burst(ICM42607P_REG_TEMP_DATA1, buf, sizeof(buf)) != 0)
    {
        return -9;
    }

    if (icm42607p_burst_is_reset(buf))
    {
        if (icm42607p_read_reg(ICM42607P_REG_PWR_MGMT0, &pwr_rb) == 0)
        {
            __ERR("icm42607p accel data invalid after init, pwr=0x%02x\n", pwr_rb);
        }
        else
        {
            __ERR("icm42607p accel data invalid after init\n");
        }
        return -10;
    }

    if (icm42607p_read_reg(ICM42607P_REG_PWR_MGMT0, &pwr_rb) != 0)
    {
        pwr_rb = 0;
    }

    __INFO("icm42607p init ok, pwr=0x%02x accel=%d gyro=%d\n",
           pwr_rb, s_accel_enabled, s_gyro_enabled);
    return 0;
}

int icm42607p_read_all(icm42607p_vec_t *accel, icm42607p_vec_t *gyro, float *temp)
{
    uint8_t buf[14];
    int16_t raw = 0;

    if (accel == NULL || gyro == NULL || temp == NULL)
    {
        return -1;
    }

    if (icm42607p_read_burst(ICM42607P_REG_TEMP_DATA1, buf, sizeof(buf)) != 0)
    {
        return -1;
    }

    if (s_accel_enabled && icm42607p_burst_is_reset(buf))
    {
        return -1;
    }

    raw = icm42607p_to_int16(buf[0], buf[1]);
    if (!icm42607p_data_is_reset(raw))
    {
        *temp = (float)raw / 128.0f + 25.0f;
    }
    else
    {
        *temp = 0.0f;
    }

    if (s_accel_enabled)
    {
        accel->x = (float)icm42607p_to_int16(buf[2], buf[3]) / s_accel_sensitivity;
        accel->y = (float)icm42607p_to_int16(buf[4], buf[5]) / s_accel_sensitivity;
        accel->z = (float)icm42607p_to_int16(buf[6], buf[7]) / s_accel_sensitivity;
    }
    else
    {
        accel->x = 0.0f;
        accel->y = 0.0f;
        accel->z = 0.0f;
    }

    if (s_gyro_enabled &&
        !icm42607p_data_is_reset(icm42607p_to_int16(buf[8], buf[9])))
    {
        gyro->x = (float)icm42607p_to_int16(buf[8], buf[9]) / s_gyro_sensitivity;
        gyro->y = (float)icm42607p_to_int16(buf[10], buf[11]) / s_gyro_sensitivity;
        gyro->z = (float)icm42607p_to_int16(buf[12], buf[13]) / s_gyro_sensitivity;
    }
    else
    {
        gyro->x = 0.0f;
        gyro->y = 0.0f;
        gyro->z = 0.0f;
    }

    return 0;
}
