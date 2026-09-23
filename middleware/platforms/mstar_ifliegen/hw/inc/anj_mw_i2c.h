#ifndef __ANJ_MW_I2C_H__
#define __ANJ_MW_I2C_H__

#define I2C_W               0
#define I2C_R               1
#define I2C_DELAYTIME       5
#define I2C_TPT29555_ADDR   0x20


/*
    @brief 设置扩展GPIO口的方向
    @param port 端口号(0-1)或引脚号(0-15)
    @param value =0输出，=1输入（mode = 0时每个bit对应一个引脚值）
    @param mode =0获取port所有引脚的值，=1获取单个引脚的值
*/

int anj_i2c_expand_gpio_direc_set(int port, int mode, unsigned char value);

/*
    @brief 获取扩展GPIO口的值
    @param port 端口号(0-1)或引脚号(0-15)
    @param value =0低电平，=1高电平（mode = 0时每个bit对应一个引脚值）
    @param mode =0获取port所有引脚的值，=1获取单个引脚的值
*/
int anj_i2c_expand_gpio_value_get(int port, int mode, unsigned char *value);

/*
    @brief 设置扩展GPIO口的值
    @param port 端口号(0-1)或引脚号(0-15)
    @param value =0低电平，=1高电平（mode = 0时每个bit对应一个引脚值）
    @param mode =0 设置port所有引脚的值，=1设置单个引脚的值
*/
int anj_i2c_expand_gpio_value_set(int port, int mode, unsigned char value);

int anj_i2c_expand_gpio_init(int sda_port, int scl_port);

int anj_i2c_expand_gpio_uninit();


#endif
