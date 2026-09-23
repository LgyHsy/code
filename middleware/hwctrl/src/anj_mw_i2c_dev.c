#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include "anj_mw_log.h"
#include "anj_mw_mutex.h"
#include "anj_mw_i2c_dev.h"

int anj_i2c_dev_open(anj_i2c_dev_t *dev, const char *path, uint8_t addr)
{
    if (dev == NULL || path == NULL)
    {
        return -1;
    }

    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
    pthread_mutex_init(&dev->lock, NULL);

    dev->fd = open(path, O_RDWR);
    if (dev->fd < 0)
    {
        __ERR("i2c open %s failed: %s\n", path, strerror(errno));
        return -1;
    }

    if (ioctl(dev->fd, I2C_SLAVE, addr) < 0)
    {
        __ERR("i2c set slave 0x%02x failed: %s\n", addr, strerror(errno));
        close(dev->fd);
        dev->fd = -1;
        return -1;
    }

    dev->addr = addr;
    return 0;
}

void anj_i2c_dev_close(anj_i2c_dev_t *dev)
{
    if (dev == NULL)
    {
        return;
    }

    if (dev->fd >= 0)
    {
        close(dev->fd);
        dev->fd = -1;
    }

    pthread_mutex_destroy(&dev->lock);
    memset(dev, 0, sizeof(*dev));
    dev->fd = -1;
}

int anj_i2c_dev_write_reg(anj_i2c_dev_t *dev, uint8_t reg, const uint8_t *data, uint8_t len)
{
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data rdwr;
    uint8_t *buf = NULL;
    int ret = 0;

    if (dev == NULL || dev->fd < 0 || (len > 0 && data == NULL))
    {
        return -1;
    }

    buf = (uint8_t *)malloc(len + 1);
    if (buf == NULL)
    {
        return -1;
    }

    buf[0] = reg;
    if (len > 0)
    {
        memcpy(buf + 1, data, len);
    }

    msg.addr = dev->addr;
    msg.flags = 0;
    msg.len = (uint16_t)(len + 1);
    msg.buf = buf;

    rdwr.msgs = &msg;
    rdwr.nmsgs = 1;

    anj_mutex_lock(&dev->lock);
    ret = ioctl(dev->fd, I2C_RDWR, &rdwr);
    anj_mutex_unlock(&dev->lock);
    free(buf);

    if (ret < 0)
    {
        __ERR("i2c write reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }

    return 0;
}

int anj_i2c_dev_read_reg(anj_i2c_dev_t *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data rdwr;
    uint8_t reg_addr = reg;
    int ret = 0;

    if (dev == NULL || dev->fd < 0 || data == NULL || len == 0)
    {
        return -1;
    }

    msgs[0].addr = dev->addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg_addr;

    msgs[1].addr = dev->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = data;

    rdwr.msgs = msgs;
    rdwr.nmsgs = 2;

    anj_mutex_lock(&dev->lock);
    ret = ioctl(dev->fd, I2C_RDWR, &rdwr);
    anj_mutex_unlock(&dev->lock);

    if (ret < 0)
    {
        __ERR("i2c read reg 0x%02x failed: %s\n", reg, strerror(errno));
        return -1;
    }

    return 0;
}
