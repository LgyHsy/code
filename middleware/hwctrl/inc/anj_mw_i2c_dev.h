#ifndef __ANJ_MW_I2C_DEV_H__
#define __ANJ_MW_I2C_DEV_H__

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int fd;
    uint8_t addr;
    pthread_mutex_t lock;
} anj_i2c_dev_t;

int anj_i2c_dev_open(anj_i2c_dev_t *dev, const char *path, uint8_t addr);
void anj_i2c_dev_close(anj_i2c_dev_t *dev);
int anj_i2c_dev_write_reg(anj_i2c_dev_t *dev, uint8_t reg, const uint8_t *data, uint8_t len);
int anj_i2c_dev_read_reg(anj_i2c_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
