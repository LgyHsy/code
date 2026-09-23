#include <linux/delay.h>
#include <linux/kernel.h>

#include "anj_ptziic_gc615.h"
#include "anj_ptziic_i2c.h"

#define GC6153E_ID (0x53)

/* 寄存器 */
#define REG_A_MODE (0x00)   /* A 通道波形与细分 */
#define REG_A_CURRENT (0x01)/* A 通道电流 */
#define REG_A_CYCLE_L (0x02)
#define REG_A_CYCLE_H (0x03)
#define REG_A_START_POS (0x04)
#define REG_A_BEXC (0x05)
#define REG_A_PS (0x06)     /* A 通道电源开关 */
#define REG_A_PULSE_H (0x07)/* A 通道 UPDW 复用寄存器高位 */
#define REG_A_PULSE_L (0x08)
#define REG_B_MODE (0x09)
#define REG_B_CURRENT (0x0a)
#define REG_B_CYCLE_L (0x0b)
#define REG_B_CYCLE_H (0x0c)
#define REG_B_START_POS (0x0d)
#define REG_B_BEXC (0x0e)
#define REG_B_PS (0x0f)
#define REG_B_PULSE_H (0x10)
#define REG_B_PULSE_L (0x11)
#define REG_MODE_SEL (0x12)
#define REG_CHOP (0x13)
#define REG_CLK_TRIM (0x14)
#define REG_CH5_CTL (0x16) /* IRCUT 通道 */
#define REG_STB (0x17)     /* STB / 计步清零 / 寄存器初始化 */
#define REG_A_STEP_H (0x18)
#define REG_A_STEP_L (0x19)
#define REG_B_STEP_H (0x1a)
#define REG_B_STEP_L (0x1b)
#define REG_CHIP_ID (0x1d)

/* REG_STB 取值 */
#define STB_INIT (0x00)      /* 输出高阻 + 清计步 + 寄存器初始化 */
#define STB_ON (0x1f)        /* 正常工作 */
#define STB_HIZ (0x03)       /* 通道输出高阻 */
#define STB_CLEAR_A (0x1b)   /* A 计步寄存器清零 */
#define STB_CLEAR_B (0x17)   /* B 计步寄存器清零 */

/* REG_CH5_CTL 取值 */
#define CH5_FORWARD (0x01)
#define CH5_REVERSE (0x02)
#define CH5_BRAKE (0x03)

#define MOTOR_RUN_CURRENT (0x7f)
#define MOTOR_PS_OFF (0x03)

static u8 s_ucChipAddr = 0x20;
static u8 s_ucChipId = 0;

static int gc615_write(u8 reg, u8 val)
{
    return anj_ptziic_i2c_write_reg(s_ucChipAddr, reg, val);
}

static u8 gc615_read(u8 reg)
{
    return anj_ptziic_i2c_read_reg(s_ucChipAddr, reg);
}

u8 gc615_chip_id(void)
{
    return s_ucChipId;
}

int gc615_config(u8 chip_addr)
{
    int iRet = 0;

    s_ucChipAddr = chip_addr;
    s_ucChipId = gc615_read(REG_CHIP_ID);
    if (s_ucChipId != GC6153E_ID)
    {
        printk(KERN_ERR "anjgc615: bad chip id 0x%02x, expect 0x%02x\n", s_ucChipId, GC6153E_ID);
        return -1;
    }

    iRet |= gc615_write(REG_STB, STB_INIT);
    udelay(10);
    iRet |= gc615_write(REG_STB, STB_ON);

    iRet |= gc615_write(REG_MODE_SEL, 0xc4); /* 自主模式 + UPDW 加减速 + 两相四线 */
    iRet |= gc615_write(REG_CHOP, 0x33);
    iRet |= gc615_write(REG_CLK_TRIM, 0x0f);

    iRet |= gc615_write(REG_A_MODE, 0x00); /* 微步模式，4 细分 */
    iRet |= gc615_write(REG_B_MODE, 0x10);

    iRet |= gc615_write(REG_A_CURRENT, MOTOR_RUN_CURRENT);
    iRet |= gc615_write(REG_B_CURRENT, MOTOR_RUN_CURRENT);

    iRet |= gc615_write(REG_A_CYCLE_H, 0x04);
    iRet |= gc615_write(REG_A_CYCLE_L, 0x08);
    iRet |= gc615_write(REG_B_CYCLE_H, 0x04);
    iRet |= gc615_write(REG_B_CYCLE_L, 0x08);

    iRet |= gc615_write(REG_A_BEXC, 0x08);
    iRet |= gc615_write(REG_B_BEXC, 0x08);

    iRet |= gc615_write(REG_A_START_POS, 0x00);
    iRet |= gc615_write(REG_B_START_POS, 0x00);

    iRet |= gc615_write(REG_A_PS, 0x02); /* 打开步进驱动电源 */
    iRet |= gc615_write(REG_B_PS, 0x02);

    iRet |= gc615_write(REG_CH5_CTL, CH5_BRAKE);
    iRet |= gc615_write(REG_STB, STB_HIZ);

    if (iRet != 0)
    {
        printk(KERN_ERR "anjgc615: config write failed\n");
        return -1;
    }

    if (gc615_read(REG_A_MODE) != 0x00 || gc615_read(REG_B_MODE) != 0x10 ||
        gc615_read(REG_CH5_CTL) != CH5_BRAKE)
    {
        printk(KERN_ERR "anjgc615: config verify failed\n");
        return -1;
    }

    return 0;
}

/*
 * UPDW 复用寄存器：五档频率 + 加减速步长 F + 恒速步长 G(低 10 位/高 6 位)，
 * 共八次配置必须连续写完，中间不能插其它寄存器操作。
 * 这里固定 F=0（不使用加减速），G 即为总微步数。
 */
static void gc615_updw_write(int motor, int dir, u32 period, u32 pulses)
{
    u8 ucRegH = (motor == GC615_MOTOR_A) ? REG_A_PULSE_H : REG_B_PULSE_H;
    u8 ucRegL = (motor == GC615_MOTOR_A) ? REG_A_PULSE_L : REG_B_PULSE_L;
    u8 ucRt = (dir == GC615_DIR_REVERSE) ? 1 : 0;
    u32 auiSpeed[5] = {0};
    int i = 0;

    /* 第 5 档为目标速度，往前每档慢 50%，构成加速起步 */
    auiSpeed[4] = period;
    auiSpeed[3] = auiSpeed[4] * 3 / 2;
    auiSpeed[2] = auiSpeed[3] * 3 / 2;
    auiSpeed[1] = auiSpeed[2] * 3 / 2;
    auiSpeed[0] = auiSpeed[1] * 3 / 2;

    for (i = 0; i < 5; i++)
    {
        u8 ucEn = (i == 0) ? 1 : 0;
        gc615_write(ucRegH, (0x03 & (auiSpeed[i] >> 8)) | (ucEn << 7) | (ucRt << 6));
        gc615_write(ucRegL, (u8)auiSpeed[i]);
    }

    /* F = 0 */
    gc615_write(ucRegH, ucRt << 6);
    gc615_write(ucRegL, 0x00);

    /* G 低 10 位 */
    gc615_write(ucRegH, (0x03 & (pulses >> 8)) | (ucRt << 6));
    gc615_write(ucRegL, (u8)pulses);

    /* G 高 6 位 */
    gc615_write(ucRegH, ucRt << 6);
    gc615_write(ucRegL, (u8)(pulses >> 10));
}

int gc615_motor_run(int motor, int dir, u32 period, u32 pulses)
{
    u8 ucPsReg = (motor == GC615_MOTOR_A) ? REG_A_PS : REG_B_PS;
    u8 ucCurReg = (motor == GC615_MOTOR_A) ? REG_A_CURRENT : REG_B_CURRENT;

    if (pulses == 0)
    {
        return -1;
    }

    if (gc615_write(ucPsReg, MOTOR_PS_OFF) != 0)
    {
        return -1;
    }
    gc615_write(REG_STB, STB_ON);
    gc615_write(ucCurReg, MOTOR_RUN_CURRENT);

    gc615_updw_write(motor, dir, period, pulses);

    return 0;
}

int gc615_motor_stop(int motor)
{
    return gc615_write((motor == GC615_MOTOR_A) ? REG_A_PS : REG_B_PS, MOTOR_PS_OFF);
}

int gc615_steps_clear(int motor)
{
    int iRet = 0;

    iRet |= gc615_write(REG_STB, (motor == GC615_MOTOR_A) ? STB_CLEAR_A : STB_CLEAR_B);
    iRet |= gc615_write(REG_STB, STB_ON);

    return (iRet == 0) ? 0 : -1;
}

u32 gc615_steps_read(int motor)
{
    u8 ucRegH = (motor == GC615_MOTOR_A) ? REG_A_STEP_H : REG_B_STEP_H;
    u8 ucRegL = (motor == GC615_MOTOR_A) ? REG_A_STEP_L : REG_B_STEP_L;
    u32 uiSteps = 0;

    uiSteps = gc615_read(ucRegH);
    uiSteps = uiSteps << 8;
    uiSteps |= gc615_read(ucRegL);

    return uiSteps;
}

int gc615_ircut_drive(int day)
{
    int iRet = 0;

    iRet |= gc615_write(REG_STB, STB_ON);
    iRet |= gc615_write(REG_CH5_CTL, day ? CH5_FORWARD : CH5_REVERSE);

    return (iRet == 0) ? 0 : -1;
}

int gc615_ircut_brake(void)
{
    return gc615_write(REG_CH5_CTL, CH5_BRAKE);
}

int gc615_output_disable(void)
{
    return gc615_write(REG_STB, STB_HIZ);
}
