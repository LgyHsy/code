#ifndef __ICM42607P_H__
#define __ICM42607P_H__

#include <stdint.h>

#define ICM42607P_I2C_ADDR               0x69
#define ICM42607P_I2C_BUS_ID                1

#define ICM42607P_REG_SIGNAL_PATH_RESET  0x02
#define ICM42607P_SOFT_RESET_DEVICE      0x10

#define ICM42607P_REG_MCLK_RDY       0x00
#define ICM42607P_REG_WHO_AM_I       0x75
#define ICM42607P_REG_PWR_MGMT0      0x1F
#define ICM42607P_REG_DRIVE_CONFIG2  0x04
#define ICM42607P_REG_INTF_CONFIG0   0x35
#define ICM42607P_REG_INTF_CONFIG1   0x36
#define ICM42607P_REG_INT_STATUS     0x3A
#define ICM42607P_REG_GYRO_CONFIG0   0x20
#define ICM42607P_REG_ACCEL_CONFIG0  0x21
#define ICM42607P_REG_GYRO_CONFIG1   0x23
#define ICM42607P_REG_ACCEL_CONFIG1  0x24
#define ICM42607P_REG_TEMP_DATA1     0x09
#define ICM42607P_REG_ACCEL_DATA_X1  0x0B
#define ICM42607P_REG_GYRO_DATA_X1   0x11

#define ICM42607P_ID                 0x60

/* PWR_MGMT0: GYRO/ACCEL mode 11 = LN (DS-000417), 01 = Gyro Standby, 10 = Accel LP */
#define PWR_MGMT0_ACCEL_LP          0x02
#define PWR_MGMT0_ACCEL_LN          0x03
#define PWR_MGMT0_GYRO_STANDBY      0x04
#define PWR_MGMT0_GYRO_LN           0x0C
#define PWR_MGMT0_IDLE_ON           0x10
#define PWR_MGMT0_SENSOR_LN         (PWR_MGMT0_ACCEL_LN | PWR_MGMT0_GYRO_LN)
#define PWR_MGMT0_LN_ENABLE         PWR_MGMT0_SENSOR_LN

#define ICM42607P_INT_STATUS_RESET_DONE  0x10
#define ICM42607P_INTF_CONFIG0_SPI_DIS   0x02
#define ICM42607P_FILTER_BW_25HZ         0x06

#define ICM42607P_MCLK_RDY_MASK      0x01
#define ICM42607P_MCLK_RDY_ALT_MASK  0x08
#define ICM42607P_DATA_RESET_RAW     ((int16_t)0x8000)

#define ICM42607P_MCLK_RDY_TIMEOUT_MS     100
#define ICM42607P_RESET_DONE_TIMEOUT_MS  100
#define ICM42607P_MODE_SETTLE_US         250
#define ICM42607P_REG_WRITE_DELAY_US     50
#define ICM42607P_GYRO_STANDBY_MS        45

#define GYRO_FS_2000DPS             0x00
#define GYRO_FS_1000DPS             0x01
#define GYRO_FS_500DPS              0x02
#define GYRO_FS_250DPS              0x03

#define GYRO_ODR_100HZ              0x09

#define ACCEL_FS_16G                0x00
#define ACCEL_FS_8G                 0x01
#define ACCEL_FS_4G                 0x02
#define ACCEL_FS_2G                 0x03

#define ACCEL_ODR_100HZ             0x09

#define ICM42607P_POWER_ON_DELAY_MS  100
#define ICM42607P_GYRO_STARTUP_MS    60
#define ICM42607P_SAMPLE_INTERVAL_MS 100

#define ICM42607P_PWR_MODE_LP          0 // low power mode
#define ICM42607P_PWR_MODE_LN          1 // low noise mode

#ifndef ICM42607P_PWR_MODE
#define ICM42607P_PWR_MODE             ICM42607P_PWR_MODE_LP
#endif

typedef struct
{
    float x;
    float y;
    float z;
} icm42607p_vec_t;

int icm42607p_open(const char *dev_path, uint8_t addr);
void icm42607p_close(void);
int icm42607p_init(void);
int icm42607p_whoami(uint8_t *id);
int icm42607p_read_all(icm42607p_vec_t *accel, icm42607p_vec_t *gyro, float *temp);

#endif
