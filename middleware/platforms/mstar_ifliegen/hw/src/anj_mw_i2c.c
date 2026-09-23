
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#include "anj_mw_log.h"
#include "anj_mw_file.h"
#include "anj_mw_mutex.h"

#include "anj_mw_gpio.h"
#include "anj_mw_i2c.h"

typedef struct
{
    int sda_pin;
    int scl_pin;
    FILE *fp_sda_w;
    FILE *fp_scl_w;
}i2c_expand_gpio_config_t;

typedef struct
{
    unsigned char port0_value;
    unsigned char port1_value;
}i2c_port_status_t;

static i2c_expand_gpio_config_t s_I2cGpioHandle = {0};
static i2c_port_status_t s_I2cStatus = {0};
static pthread_mutex_t s_i2c_status_mutex = PTHREAD_MUTEX_INITIALIZER;

static void i2c_sleep_ms(unsigned long milliseconds)
{
    struct timespec ts = {0};
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

static void i2c_sleep_us(unsigned long microseconds)
{
    struct timespec ts = {0};
    ts.tv_sec = microseconds / 1000 / 1000;
    ts.tv_nsec = (microseconds % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static char get_char_one_bit(char data, char num)
{
    return data >> num & 1;
}

static char set_char_one_bit(char data, char num, char val)
{
    char ret = 0;
    if(val)
    {
        ret = data | (1 << num);
    }
    else
    {
        ret = data & (~(1 << num));
    }

    return ret;
}

static int anj_i2c_gpio_direc_set(int port, int direc)
{
    char direction[10] = {0};
    char filename[64] = {0};
    snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%d/direction", port);

    if (direc == GPIO_DIREC_IN)
    {
        strcpy(direction, "in");
    }
    else if (direc == GPIO_DIREC_OUT)
    {
        strcpy(direction, "out");
    }
    else
    {
        return -1;
    }

    FILE *fp = anj_mw_fopen(filename, "w");
    if (fp == NULL)
    {
        return -1;
    }

    anj_mw_fwrite(fp, direction, strlen(direction));
    anj_mw_fclose(fp);

    return 0;
}

int anj_i2c_gpio_value_set(int port, int value)
{
    FILE *fp = NULL;
    if (port == s_I2cGpioHandle.scl_pin)
    {
        fp = s_I2cGpioHandle.fp_scl_w;
    }
    else if (port == s_I2cGpioHandle.sda_pin)
    {
        fp = s_I2cGpioHandle.fp_sda_w;
    }

    if (fp == NULL)
    {
        __ERR("i2c port:%d path error\n", port);
        return -1;
    }

    fprintf(fp, "%d", value);
    anj_mw_fflush(fp);

    return 0;
}

unsigned char anj_i2c_gpio_value_get(int port)
{
    unsigned char value = 0;

    char filename[64] = {0};
    snprintf(filename, sizeof(filename), "/sys/class/gpio/gpio%d/value", port);

    char buf[16] = {0};
    FILE *fp = anj_mw_fopen(filename, "r");
    if (fp == NULL)
    {
        return 0;
    }

    anj_mw_fread(fp, buf, sizeof(buf));
    anj_mw_fclose(fp);

    int tmp_value = atoi(buf);
    if (tmp_value == GPIO_VALUE_HIGH)
    {
        value = 1;
    }
    else
    {
        value = 0;
    }

    return value;
}

void anj_i2c_start()
{
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_OUT);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);
    i2c_sleep_us(I2C_DELAYTIME);
    anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_HIGH);
    i2c_sleep_us(I2C_DELAYTIME);

	anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_LOW);
	i2c_sleep_us(I2C_DELAYTIME);

	anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
	i2c_sleep_us(I2C_DELAYTIME);
}

void anj_i2c_stop()
{
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_OUT);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);
    anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_HIGH);
    i2c_sleep_us(I2C_DELAYTIME);
}


void anj_i2c_send_data(unsigned char data)
{
    int i = 0;
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_OUT);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    for (i = 0; i < 8; i++)
    {
        if (data & (0x80 >> i))
        {
            anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_HIGH);
        }
        else
        {
            anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_LOW);
        }

        anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);
        i2c_sleep_us(I2C_DELAYTIME);

        anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
        i2c_sleep_us(I2C_DELAYTIME); 
    }
}


unsigned char anj_i2c_recv_data(void)
{
    int i = 0;
    unsigned char data = 0;

    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_IN);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    for (i = 0; i < 8; i++)
    {
        anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);
        i2c_sleep_us(I2C_DELAYTIME);
        data <<= 1;
        data |= anj_i2c_gpio_value_get(s_I2cGpioHandle.sda_pin);
        anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
        i2c_sleep_us(I2C_DELAYTIME);
    }

    return data;
}

//发送应答信号(0x0:ACK 0x01:NACK)
void anj_i2c_send_ack(unsigned char ack)
{
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_OUT);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    if (ack)
    {
        anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_HIGH);
    }
    else
    {
        anj_i2c_gpio_value_set(s_I2cGpioHandle.sda_pin, GPIO_VALUE_LOW);
    }

    i2c_sleep_us(I2C_DELAYTIME);
    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);

    i2c_sleep_us(I2C_DELAYTIME);
    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);
}

int anj_i2c_recv_ack(void)
{
    int cnt = 0;
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.scl_pin, GPIO_DIREC_OUT);
    anj_i2c_gpio_direc_set(s_I2cGpioHandle.sda_pin, GPIO_DIREC_IN);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_HIGH);
    i2c_sleep_us(I2C_DELAYTIME);

    while(anj_i2c_gpio_value_get(s_I2cGpioHandle.sda_pin))
    {
        cnt++;
        if (cnt >= 10)
        {
            return 1;
        }
    }

    anj_i2c_gpio_value_set(s_I2cGpioHandle.scl_pin, GPIO_VALUE_LOW);
    i2c_sleep_us(I2C_DELAYTIME);

    return 0;
}

int anj_i2c_reg_value_get(unsigned char reg_addr, unsigned char *value)
{
    anj_i2c_start();
    anj_i2c_send_data(I2C_TPT29555_ADDR << 1 | I2C_W);
    if (anj_i2c_recv_ack())
    {
        anj_i2c_stop();
        __INFO("i2c gpio reg get value not recv ack!\n");
        return 1;
    }

    anj_i2c_send_data(reg_addr);
    anj_i2c_recv_ack();
    i2c_sleep_us(I2C_DELAYTIME);

    anj_i2c_start();
    anj_i2c_send_data(I2C_TPT29555_ADDR << 1 | I2C_R);
    anj_i2c_recv_ack();

    *value = anj_i2c_recv_data();
    anj_i2c_send_ack(1);

    anj_i2c_stop();
    return 0;
}

int anj_i2c_reg_value_set(unsigned char reg_addr, unsigned char value)
{
    anj_i2c_start();
    anj_i2c_send_data(I2C_TPT29555_ADDR << 1 | I2C_W);
    if (anj_i2c_recv_ack())
    {
        anj_i2c_stop();
        __INFO("i2c gpio reg get value not recv ack!\n");
        return 1;
    }

    anj_i2c_send_data(reg_addr);
    anj_i2c_recv_ack();

    anj_i2c_send_data(value);
    anj_i2c_recv_ack();

    anj_i2c_stop();
    return 0;
}


int anj_i2c_expand_gpio_direc_set(int port, int mode, unsigned char value)
{
    unsigned char tmp_value = 0;
    unsigned char data = 0;

    anj_mutex_lock(&s_i2c_status_mutex);

    if (mode)
    {
        if (port)
        {
            anj_i2c_reg_value_get(0x07, &tmp_value);
            data = set_char_one_bit(tmp_value, port - 8, value);
            anj_i2c_reg_value_set(0x07, data);
        }
        else
        {
            anj_i2c_reg_value_get(0x06, &tmp_value);
            data = set_char_one_bit(tmp_value, port, value);
            anj_i2c_reg_value_set(0x06, data);
        }
    }
    else
    {
        if (port)
        {
            anj_i2c_reg_value_set(0x07, value);
        }
        else
        {
            anj_i2c_reg_value_set(0x06, value);
        }
    }

    anj_mutex_unlock(&s_i2c_status_mutex);
    return 0;
}

/*
    @brief 获取扩展GPIO口的值
    @param port 端口号(0-1)或引脚号(0-15)
    @param value =0低电平，=1高电平（mode = 0时每个bit对应一个引脚值）
    @param mode =0获取port所有引脚的值，=1获取单个引脚的值
*/
int anj_i2c_expand_gpio_value_get(int port, int mode, unsigned char *value)
{
    unsigned char uTmpValue = 0;

    anj_mutex_lock(&s_i2c_status_mutex);

    if (mode)
    {
        if (port >= 8)
        {
            anj_i2c_reg_value_get(0x01, &uTmpValue);
            *value = get_char_one_bit(uTmpValue, port - 8);
        }
        else
        {
            anj_i2c_reg_value_get(0x00, &uTmpValue);
            *value = get_char_one_bit(uTmpValue, port);
        }
    }
    else
    {
        if (port)
        {
            anj_i2c_reg_value_get(0x01, value);
        }
        else
        {
            anj_i2c_reg_value_get(0x00, value);
        }
    }

    anj_mutex_unlock(&s_i2c_status_mutex);

    return 0;
}

/*
    @brief 设置扩展GPIO口的值
    @param port 端口号(0-1)或引脚号(0-15)
    @param value =0低电平，=1高电平（mode = 0时每个bit对应一个引脚值）
    @param mode =0 设置port所有引脚的值，=1设置单个引脚的值
*/
int anj_i2c_expand_gpio_value_set(int port, int mode, unsigned char value)
{
    unsigned char temp_value = 0;
    unsigned char data = 0;

    anj_mutex_lock(&s_i2c_status_mutex);

    temp_value = (port >= 8) ? s_I2cStatus.port1_value : s_I2cStatus.port0_value;
    if (mode)
    {
        if (port >= 8)
        {
            data = set_char_one_bit(temp_value, port - 8, value);
            anj_i2c_reg_value_set(0x03, data);

            s_I2cStatus.port1_value = data;
        }
        else
        {
            data = set_char_one_bit(temp_value, port, value);
            anj_i2c_reg_value_set(0x02, data);

            s_I2cStatus.port0_value = data;
        }
    }
    else
    {
        if (port)
        {
            anj_i2c_reg_value_set(0x03, value);
        }
        else
        {
            anj_i2c_reg_value_set(0x02, value);
        }
    }

    anj_mutex_unlock(&s_i2c_status_mutex);

    return 0;
}

int anj_i2c_expand_gpio_init(int sda_port, int scl_port)
{
    s_I2cGpioHandle.sda_pin = sda_port;
    s_I2cGpioHandle.scl_pin = scl_port;
    s_I2cGpioHandle.fp_scl_w = NULL;
    s_I2cGpioHandle.fp_sda_w = NULL;
    
    char sda_path[32] = {0};
    char scl_path[32] = {0};
    snprintf(sda_path, sizeof(sda_path), "/sys/class/gpio/gpio%d", sda_port);
    snprintf(scl_path, sizeof(scl_path), "/sys/class/gpio/gpio%d", scl_port);

    if (access(sda_path, F_OK) != 0 || access(scl_path, F_OK) != 0)
    {
        anj_gpio_write_port_export(scl_port);
        anj_i2c_gpio_direc_set(scl_port, GPIO_DIREC_OUT);

        anj_gpio_write_port_export(sda_port);
        anj_i2c_gpio_direc_set(sda_port, GPIO_DIREC_OUT);

        __INFO("expand gpio i2c init port:%d, %d\n", scl_port, sda_port);
        i2c_sleep_ms(1000);
    }
    else
    {
        __INFO("expand gpio i2c init port:%d, %d\n", scl_port, sda_port);
    }

    char sda_value[64] = {0};
    char scl_value[64] = {0};
    snprintf(sda_value, sizeof(sda_value), "%s/value", sda_path);
    snprintf(scl_value, sizeof(scl_value), "%s/value", scl_path);

    s_I2cGpioHandle.fp_scl_w = anj_mw_fopen(scl_value, "w+");
    if (s_I2cGpioHandle.fp_scl_w == NULL)
    {
        __ERR("i2c open scl path:%s failed!\n", scl_value);
        return -1;
    }
    s_I2cGpioHandle.fp_sda_w = anj_mw_fopen(sda_value, "w+");
    if (s_I2cGpioHandle.fp_sda_w == NULL)
    {
        __ERR("i2c open fp_sda path:%s failed!\n", sda_value);
        anj_mw_fclose(s_I2cGpioHandle.fp_scl_w);
        return -1;
    }

    return 0;
}


int anj_i2c_expand_gpio_uninit()
{
    if (s_I2cGpioHandle.fp_scl_w != NULL)
    {
        anj_mw_fclose(s_I2cGpioHandle.fp_scl_w);
        s_I2cGpioHandle.fp_scl_w = NULL;
    }

    if (s_I2cGpioHandle.fp_sda_w != NULL)
    {
        anj_mw_fclose(s_I2cGpioHandle.fp_sda_w);
        s_I2cGpioHandle.fp_sda_w = NULL;
    }

    s_I2cGpioHandle.sda_pin = 0;
    s_I2cGpioHandle.scl_pin = 0;

    return 0;
}

