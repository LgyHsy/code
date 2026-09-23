/*
 * GC6153E 云台驱动：软 I2C + 双通道步进电机 + IRCUT
 *
 * 电机 ioctl 命令号与 module/ptz/inc/anj_ptz.h 的 PTZ_CTL_* 保持一致。
 * 引脚由 insmod 参数传入，例如：
 *     insmod anj_ptziic.ko gpio_scl=10 gpio_sda=11 gpio_rst=9
 */
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "anj_ptziic_gc615.h"
#include "anj_ptziic_i2c.h"

#define ANJ_PTZIIC_DEV_NAME "anjgc615"
#define ANJ_PTZIIC_MAJOR (97)
#define ANJ_PTZIIC_MINORS (1)

/* TODO: 硬件未定，确认后改这里的默认值即可 */
#define GC615_GPIO_SCL_DEF (-1)
#define GC615_GPIO_SDA_DEF (-1)
#define GC615_GPIO_RST_DEF (-1)
#define GC615_I2C_ADDR_DEF (0x20)
#define GC615_STEPS_PER_PTZ_DEF (16) /* 1 产品步 = 8拍 = 4整步 = 16微步(4细分) */

/* 与 module/ptz/inc/anj_ptz.h 一致，勿改数值 */
enum
{
    PTZ_CTL_MOTOR_UP = 10,
    PTZ_CTL_MOTOR_DOWN,
    PTZ_CTL_MOTOR_LEFT,
    PTZ_CTL_MOTOR_RIGHT,
    PTZ_CTL_MOTOR_STOP,
    PTZ_CTL_REMAIN_STEP,
    PTZ_CTL_SPEED_SET,
    PTZ_CTL_HDIR_SET,
    PTZ_CTL_VDIR_SET,
    PTZ_CTL_DEBUG,
    PTZ_CTL_NULL,
};

#define ANJ_GC615_IRCUT_SET (30) /* arg: 1=DAY, 0=NIGHT */

#define PTZ_ERROR_CMD_INVALID (-1)
#define PTZ_ERROR_MOTOR_CTL_INVALID (-2)
#define PTZ_ERROR_OTHER (-99)

#define PTZ_SPEED_DEFAULT (3)

static int gpio_scl = GC615_GPIO_SCL_DEF;
static int gpio_sda = GC615_GPIO_SDA_DEF;
static int gpio_rst = GC615_GPIO_RST_DEF;
static int chip_addr = GC615_I2C_ADDR_DEF;
static int step_ratio = GC615_STEPS_PER_PTZ_DEF;
module_param(gpio_scl, int, 0644);
module_param(gpio_sda, int, 0644);
module_param(gpio_rst, int, 0644);
module_param(chip_addr, int, 0644);
module_param(step_ratio, int, 0644);

/* PTZ_SPEED_1..10 → UPDW 周期值，越大越慢 */
static const u32 s_auiSpeedPeriod[] = {
    200, 181, 163, 144, 126, 107, 88, 70, 51, 32,
};

static struct class *s_pstClass = NULL;
static struct cdev s_stCdev;
static dev_t s_stDevNo;
static DEFINE_MUTEX(s_stChipLock);  /* 保护芯片寄存器访问，只在短操作内持有 */
static DEFINE_MUTEX(s_stIrcutLock); /* 串行化整个 IRCUT 翻转流程 */

static int s_iRunMotor = -1; /* <0 表示空闲 */
static int s_iRunSteps = 0;  /* 本次命令下发的产品步 */
static u32 s_uiPeriod = 0;
static int s_iHDir = 0;
static int s_iVDir = 0;
static int s_iIrcutDay = -1; /* -1 表示未知 */
static int s_iIrcutBusy = 0; /* 翻转维持中，此时不能让通道进高阻 */

static void anj_ptziic_idle_locked(void)
{
    if (s_iRunMotor < 0 && !s_iIrcutBusy)
    {
        gc615_output_disable();
    }
}

static void anj_ptziic_run_done_locked(void)
{
    gc615_steps_clear(s_iRunMotor);
    s_iRunMotor = -1;
    s_iRunSteps = 0;
    anj_ptziic_idle_locked();
}

/* 返回剩余产品步，恒为 >=0；走完顺带收尾 */
static int anj_ptziic_remain_locked(void)
{
    int iDone = 0;
    int iRemain = 0;

    if (s_iRunMotor < 0)
    {
        return 0;
    }

    iDone = (int)(gc615_steps_read(s_iRunMotor) / step_ratio);
    iRemain = s_iRunSteps - iDone;
    if (iRemain <= 0)
    {
        anj_ptziic_run_done_locked();
        return 0;
    }

    return iRemain;
}

static int anj_ptziic_motor_start_locked(int motor, int dir, int steps)
{
    int iRemain = anj_ptziic_remain_locked();

    if (iRemain > 0)
    {
        printk(KERN_ERR "anjgc615: motor busy, motor:%d remain:%d\n", s_iRunMotor, iRemain);
        return PTZ_ERROR_MOTOR_CTL_INVALID;
    }

    if (gc615_steps_clear(motor) != 0)
    {
        return PTZ_ERROR_OTHER;
    }

    if (gc615_motor_run(motor, dir, s_uiPeriod, (u32)steps * (u32)step_ratio) != 0)
    {
        return PTZ_ERROR_OTHER;
    }

    s_iRunMotor = motor;
    s_iRunSteps = steps;

    return 0;
}

static int anj_ptziic_motor_stop_locked(void)
{
    int iDone = 0;
    int iRemain = 0;

    if (s_iRunMotor < 0)
    {
        return 0;
    }

    gc615_motor_stop(s_iRunMotor);

    iDone = (int)(gc615_steps_read(s_iRunMotor) / step_ratio);
    iRemain = s_iRunSteps - iDone;
    if (iRemain < 0)
    {
        iRemain = 0;
    }

    anj_ptziic_run_done_locked();

    return iRemain;
}

static int anj_ptziic_ircut_set(int day)
{
    int iRet = 0;

    mutex_lock(&s_stIrcutLock);

    mutex_lock(&s_stChipLock);
    if (s_iIrcutDay == day)
    {
        mutex_unlock(&s_stChipLock);
        mutex_unlock(&s_stIrcutLock);
        return 0;
    }

    iRet = gc615_ircut_drive(day);
    if (iRet == 0)
    {
        s_iIrcutBusy = 1;
    }
    mutex_unlock(&s_stChipLock);

    if (iRet != 0)
    {
        mutex_unlock(&s_stIrcutLock);
        return PTZ_ERROR_OTHER;
    }

    /* 线圈维持时间放在锁外，避免堵住云台的剩余步轮询 */
    msleep(GC615_IRCUT_HOLD_MS);

    mutex_lock(&s_stChipLock);
    gc615_ircut_brake();
    s_iIrcutBusy = 0;
    s_iIrcutDay = day;
    anj_ptziic_idle_locked();
    mutex_unlock(&s_stChipLock);

    mutex_unlock(&s_stIrcutLock);

    return 0;
}

/*
 * 返回值约定：内核返回负数时用户态 ioctl() 一律得到 -1，
 * 所以剩余步类命令必须返回 >=0，错误统一用负数。
 */
static long anj_ptziic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    long lRet = 0;
    int iMotor = GC615_MOTOR_A;
    int iDir = GC615_DIR_FORWARD;
    int iSteps = (int)arg;

    switch (cmd)
    {
    case PTZ_CTL_MOTOR_UP:
    case PTZ_CTL_MOTOR_DOWN:
    case PTZ_CTL_MOTOR_LEFT:
    case PTZ_CTL_MOTOR_RIGHT:
    {
        if (iSteps <= 0)
        {
            break;
        }

        if (cmd == PTZ_CTL_MOTOR_LEFT || cmd == PTZ_CTL_MOTOR_RIGHT)
        {
            int iForward = (cmd == PTZ_CTL_MOTOR_LEFT) ? 1 : 0;
            iMotor = GC615_MOTOR_A;
            iDir = (iForward ^ s_iHDir) ? GC615_DIR_FORWARD : GC615_DIR_REVERSE;
        }
        else
        {
            int iForward = (cmd == PTZ_CTL_MOTOR_UP) ? 1 : 0;
            iMotor = GC615_MOTOR_B;
            iDir = (iForward ^ s_iVDir) ? GC615_DIR_FORWARD : GC615_DIR_REVERSE;
        }

        mutex_lock(&s_stChipLock);
        lRet = anj_ptziic_motor_start_locked(iMotor, iDir, iSteps);
        mutex_unlock(&s_stChipLock);
        break;
    }
    case PTZ_CTL_MOTOR_STOP:
    {
        mutex_lock(&s_stChipLock);
        lRet = anj_ptziic_motor_stop_locked();
        mutex_unlock(&s_stChipLock);
        break;
    }
    case PTZ_CTL_REMAIN_STEP:
    {
        mutex_lock(&s_stChipLock);
        lRet = anj_ptziic_remain_locked();
        mutex_unlock(&s_stChipLock);
        break;
    }
    case PTZ_CTL_SPEED_SET:
    {
        int iSpeed = (int)arg;

        if (iSpeed < 1)
        {
            iSpeed = 1;
        }
        if (iSpeed > (int)ARRAY_SIZE(s_auiSpeedPeriod))
        {
            iSpeed = ARRAY_SIZE(s_auiSpeedPeriod);
        }

        mutex_lock(&s_stChipLock);
        s_uiPeriod = s_auiSpeedPeriod[iSpeed - 1];
        mutex_unlock(&s_stChipLock);
        break;
    }
    case PTZ_CTL_HDIR_SET:
    {
        s_iHDir = arg ? 1 : 0;
        break;
    }
    case PTZ_CTL_VDIR_SET:
    {
        s_iVDir = arg ? 1 : 0;
        break;
    }
    case PTZ_CTL_DEBUG:
    {
        mutex_lock(&s_stChipLock);
        printk(KERN_INFO "anjgc615: id:0x%02x motor:%d steps:%d period:%u "
                         "hdir:%d vdir:%d ircut:%d ratio:%d\n",
               gc615_chip_id(), s_iRunMotor, s_iRunSteps, s_uiPeriod,
               s_iHDir, s_iVDir, s_iIrcutDay, step_ratio);
        mutex_unlock(&s_stChipLock);
        break;
    }
    case ANJ_GC615_IRCUT_SET:
    {
        lRet = anj_ptziic_ircut_set(arg ? 1 : 0);
        break;
    }
    default:
    {
        printk(KERN_ERR "anjgc615: invalid cmd %u\n", cmd);
        lRet = PTZ_ERROR_CMD_INVALID;
        break;
    }
    }

    return lRet;
}

static int anj_ptziic_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int anj_ptziic_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations s_stFops = {
    .owner = THIS_MODULE,
    .open = anj_ptziic_open,
    .release = anj_ptziic_release,
    .unlocked_ioctl = anj_ptziic_ioctl,
};

static void anj_ptziic_gpio_free(void)
{
    gpio_free(gpio_scl);
    gpio_free(gpio_sda);
    gpio_free(gpio_rst);
}

static int anj_ptziic_gpio_init(void)
{
    int iRet = 0;

    iRet = gpio_request(gpio_scl, "gc615_scl");
    if (iRet < 0)
    {
        printk(KERN_ERR "anjgc615: request scl gpio %d fail %d\n", gpio_scl, iRet);
        return iRet;
    }

    iRet = gpio_request(gpio_sda, "gc615_sda");
    if (iRet < 0)
    {
        printk(KERN_ERR "anjgc615: request sda gpio %d fail %d\n", gpio_sda, iRet);
        gpio_free(gpio_scl);
        return iRet;
    }

    iRet = gpio_request(gpio_rst, "gc615_rst");
    if (iRet < 0)
    {
        printk(KERN_ERR "anjgc615: request rst gpio %d fail %d\n", gpio_rst, iRet);
        gpio_free(gpio_scl);
        gpio_free(gpio_sda);
        return iRet;
    }

    gpio_direction_output(gpio_rst, 1);
    anj_ptziic_i2c_init(gpio_scl, gpio_sda);

    /* 芯片复位 */
    gpio_set_value(gpio_rst, 0);
    mdelay(10);
    gpio_set_value(gpio_rst, 1);
    mdelay(50);

    return 0;
}

static int anj_ptziic_cdev_init(void)
{
    int iRet = 0;

    s_stDevNo = MKDEV(ANJ_PTZIIC_MAJOR, 0);
    iRet = register_chrdev_region(s_stDevNo, ANJ_PTZIIC_MINORS, ANJ_PTZIIC_DEV_NAME);
    if (iRet < 0)
    {
        printk(KERN_ERR "anjgc615: register chrdev major %d fail %d\n", ANJ_PTZIIC_MAJOR, iRet);
        return iRet;
    }

    cdev_init(&s_stCdev, &s_stFops);
    s_stCdev.owner = THIS_MODULE;
    iRet = cdev_add(&s_stCdev, s_stDevNo, ANJ_PTZIIC_MINORS);
    if (iRet < 0)
    {
        printk(KERN_ERR "anjgc615: cdev add fail %d\n", iRet);
        unregister_chrdev_region(s_stDevNo, ANJ_PTZIIC_MINORS);
        return iRet;
    }

    s_pstClass = class_create(THIS_MODULE, ANJ_PTZIIC_DEV_NAME);
    if (IS_ERR(s_pstClass))
    {
        iRet = PTR_ERR(s_pstClass);
        printk(KERN_ERR "anjgc615: class create fail %d\n", iRet);
        s_pstClass = NULL;
        cdev_del(&s_stCdev);
        unregister_chrdev_region(s_stDevNo, ANJ_PTZIIC_MINORS);
        return iRet;
    }

    device_create(s_pstClass, NULL, s_stDevNo, NULL, ANJ_PTZIIC_DEV_NAME);

    return 0;
}

static void anj_ptziic_cdev_uninit(void)
{
    if (s_pstClass)
    {
        device_destroy(s_pstClass, s_stDevNo);
        class_destroy(s_pstClass);
        s_pstClass = NULL;
    }
    cdev_del(&s_stCdev);
    unregister_chrdev_region(s_stDevNo, ANJ_PTZIIC_MINORS);
}

static int __init anj_ptziic_drv_init(void)
{
    int iRet = 0;

    if (gpio_scl < 0 || gpio_sda < 0 || gpio_rst < 0)
    {
        printk(KERN_ERR "anjgc615: gpio not configured (scl:%d sda:%d rst:%d), "
                        "use insmod anj_ptziic.ko gpio_scl=.. gpio_sda=.. gpio_rst=..\n",
               gpio_scl, gpio_sda, gpio_rst);
        return -ENODEV;
    }

    if (step_ratio <= 0)
    {
        step_ratio = GC615_STEPS_PER_PTZ_DEF;
    }
    s_uiPeriod = s_auiSpeedPeriod[PTZ_SPEED_DEFAULT - 1];

    iRet = anj_ptziic_gpio_init();
    if (iRet < 0)
    {
        return iRet;
    }

    if (gc615_config((u8)chip_addr) != 0)
    {
        anj_ptziic_gpio_free();
        return -ENODEV;
    }

    iRet = anj_ptziic_cdev_init();
    if (iRet < 0)
    {
        anj_ptziic_gpio_free();
        return iRet;
    }

    printk(KERN_INFO "anjgc615: init done, id:0x%02x scl:%d sda:%d rst:%d ratio:%d\n",
           gc615_chip_id(), gpio_scl, gpio_sda, gpio_rst, step_ratio);

    return 0;
}

static void __exit anj_ptziic_drv_exit(void)
{
    anj_ptziic_cdev_uninit();

    mutex_lock(&s_stChipLock);
    if (s_iRunMotor >= 0)
    {
        gc615_motor_stop(s_iRunMotor);
        s_iRunMotor = -1;
    }
    gc615_output_disable();
    mutex_unlock(&s_stChipLock);

    anj_ptziic_gpio_free();

    printk(KERN_INFO "anjgc615: exit\n");
}

module_init(anj_ptziic_drv_init);
module_exit(anj_ptziic_drv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GC6153E PTZ and IRCUT driver");
MODULE_VERSION("V1.00");
