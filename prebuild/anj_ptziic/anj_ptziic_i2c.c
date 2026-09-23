#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/kernel.h>

#include "anj_ptziic_i2c.h"

#define I2C_ACK_RETRY (255)

static int s_iSclGpio = -1;
static int s_iSdaGpio = -1;

static void anj_ptziic_i2c_delay(void)
{
    udelay(6);
}

static void anj_ptziic_i2c_scl_set(int level)
{
    gpio_set_value(s_iSclGpio, level ? 1 : 0);
}

static void anj_ptziic_i2c_sda_set(int level)
{
    gpio_set_value(s_iSdaGpio, level ? 1 : 0);
}

static void anj_ptziic_i2c_sda_input(void)
{
    gpio_direction_input(s_iSdaGpio);
}

static void anj_ptziic_i2c_sda_output(void)
{
    gpio_direction_output(s_iSdaGpio, 1);
}

static int anj_ptziic_i2c_sda_get(void)
{
    return gpio_get_value(s_iSdaGpio);
}

static void anj_ptziic_i2c_start(void)
{
    // SCL 为高时，SDA 从 1 掉到 0 = Start 条件
    anj_ptziic_i2c_sda_set(1);
    anj_ptziic_i2c_scl_set(1);
    anj_ptziic_i2c_delay();
    anj_ptziic_i2c_sda_set(0);
    anj_ptziic_i2c_delay();
}

static void anj_ptziic_i2c_stop(void)
{
    // SCL 为高时，SDA 从 0 升到 1 = Stop 条件
    anj_ptziic_i2c_sda_set(0);
    anj_ptziic_i2c_scl_set(1);
    anj_ptziic_i2c_delay();
    anj_ptziic_i2c_sda_set(1);
    anj_ptziic_i2c_delay();
}

static void anj_ptziic_i2c_write_byte(u8 data)
{
    int i = 0;

    for (i = 0; i < 8; i++)
    {
        // 传数据 SCL 需要一高一低 8拍=8bit
        anj_ptziic_i2c_scl_set(0);
        anj_ptziic_i2c_delay();
        anj_ptziic_i2c_sda_set(data & 0x80);
        anj_ptziic_i2c_scl_set(1);
        anj_ptziic_i2c_delay();
        data = data << 1;
    }

    // wait ack
    anj_ptziic_i2c_scl_set(0);
    anj_ptziic_i2c_delay();
    anj_ptziic_i2c_sda_set(1);
    anj_ptziic_i2c_delay();
}

static u8 anj_ptziic_i2c_read_byte(void)
{
    int i = 0;
    u8 data = 0;

    anj_ptziic_i2c_sda_input();
    anj_ptziic_i2c_delay();

    for (i = 0; i < 8; i++)
    {
        // 传数据 SCL 需要一高一低 8拍=8bit
        data = data << 1;
        anj_ptziic_i2c_scl_set(1);
        anj_ptziic_i2c_delay();
        if (anj_ptziic_i2c_sda_get())
        {
            data |= 0x01;
        }
        anj_ptziic_i2c_scl_set(0);
        anj_ptziic_i2c_delay();
    }

    anj_ptziic_i2c_sda_output();

    return data;
}

static int anj_ptziic_i2c_wait_ack(void)
{
    int i = 0;

    anj_ptziic_i2c_sda_input();

    anj_ptziic_i2c_scl_set(1);
    anj_ptziic_i2c_delay();
    while ((anj_ptziic_i2c_sda_get() == 1) && (i < I2C_ACK_RETRY))
    {
        i++;
    }

    anj_ptziic_i2c_scl_set(0);
    anj_ptziic_i2c_delay();
    anj_ptziic_i2c_sda_output();

    if (i >= I2C_ACK_RETRY)
    {
        return -1;
    }

    return 0;
}

void anj_ptziic_i2c_init(int scl_gpio, int sda_gpio)
{
    s_iSclGpio = scl_gpio;
    s_iSdaGpio = sda_gpio;

    gpio_direction_output(s_iSclGpio, 1);
    gpio_direction_output(s_iSdaGpio, 1);
}

int anj_ptziic_i2c_write_reg(u8 chip_addr, u8 reg, u8 val)
{
    int iRet = 0;

    anj_ptziic_i2c_start();

    anj_ptziic_i2c_write_byte(chip_addr & 0xfe);
    iRet |= anj_ptziic_i2c_wait_ack();

    anj_ptziic_i2c_write_byte(reg);
    iRet |= anj_ptziic_i2c_wait_ack();

    anj_ptziic_i2c_write_byte(val);
    iRet |= anj_ptziic_i2c_wait_ack();

    anj_ptziic_i2c_stop();

    return (iRet == 0) ? 0 : -1;
}

u8 anj_ptziic_i2c_read_reg(u8 chip_addr, u8 reg)
{
    u8 val = 0;

    anj_ptziic_i2c_start();

    anj_ptziic_i2c_write_byte(chip_addr & 0xfe);
    anj_ptziic_i2c_wait_ack();

    anj_ptziic_i2c_write_byte(reg);
    anj_ptziic_i2c_wait_ack();

    anj_ptziic_i2c_stop();
    anj_ptziic_i2c_start();

    anj_ptziic_i2c_write_byte(chip_addr | 0x01);
    anj_ptziic_i2c_wait_ack();

    val = anj_ptziic_i2c_read_byte();

    anj_ptziic_i2c_stop();

    return val;
}
