#ifndef __ANJ_PTZIIC_I2C_H__
#define __ANJ_PTZIIC_I2C_H__

#include <linux/types.h>

/* GPIO 模拟 I2C，引脚由调用方 request 后传入 */
void anj_ptziic_i2c_init(int scl_gpio, int sda_gpio);
int anj_ptziic_i2c_write_reg(u8 chip_addr, u8 reg, u8 val);
u8 anj_ptziic_i2c_read_reg(u8 chip_addr, u8 reg);

#endif /* __ANJ_PTZIIC_I2C_H__ */
