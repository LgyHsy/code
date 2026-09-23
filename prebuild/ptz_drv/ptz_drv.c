/******************************************************************/
//dts file:
/*
    timer_test {
        compatible = "sstar,timer";
    };
*/
/******************************************************************/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include "ms_types.h"
#include "ms_platform.h"
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>
#include <linux/delay.h>
#include "irqs.h"

#define PTZ_DEV_NAME "ptz"
#define PTZ_MAJOR (95)
#define MAX_PTZ_MINORS (1)

#define TIMER_US_HZ (12 * 1000) // 12=1us  1ms
#define MAX_MOTOR_CNT (8)       // motor ctl out1-8

#define INT_FIQ_TIMER_0_MAP (INT_FIQ_TIMER_0+32)
#define INT_FIQ_TIMER_1_MAP (INT_FIQ_TIMER_1+32)
#define INT_FIQ_TIMER_2_MAP (INT_FIQ_TIMER_2+32)
#define BIT_0    0x1
#define BIT_1    0x2
#define BIT_2    0x4
#define BIT_3    0x8
#define BIT_4    0x10
#define BIT_5    0x20
#define BIT_6    0x40
#define BIT_7    0x80
#define BIT_8    0x100
#define BIT_9    0x200
#define BIT_10    0x400
#define BIT_11    0x800
#define BIT_12    0x1000
#define BIT_13    0x2000
#define BIT_14    0x4000
#define BIT_15    0x8000

#define INFINITY_BASE_REG_RIU_PA     (0x1F000000)
#define BASE_REG_TIMER_PA             GET_REG_ADDR(INFINITY_BASE_REG_RIU_PA, 0x001800)
#define BK_REG(reg)             ((reg) << 2)

#define TIMER0_EN_REG        BK_REG(0x10)
#define TIMER0_HIT_REG       BK_REG(0x11)
#define TIMER0_MAX_L_REG     BK_REG(0x12)
#define TIMER0_MAX_H_REG     BK_REG(0x13)
#define TIMER0_CAP_L_REG     BK_REG(0x14)
#define TIMER0_CAP_H_REG     BK_REG(0x15)

#define TIMER1_EN_REG        BK_REG(0x20)
#define TIMER1_HIT_REG       BK_REG(0x21)
#define TIMER1_MAX_L_REG     BK_REG(0x22)
#define TIMER1_MAX_H_REG     BK_REG(0x23)
#define TIMER1_CAP_L_REG     BK_REG(0x24)
#define TIMER1_CAP_H_REG     BK_REG(0x25)

#define TIMER2_EN_REG        BK_REG(0x30)
#define TIMER2_HIT_REG       BK_REG(0x31)
#define TIMER2_MAX_L_REG     BK_REG(0x32)
#define TIMER2_MAX_H_REG     BK_REG(0x33)
#define TIMER2_CAP_L_REG     BK_REG(0x34)
#define TIMER2_CAP_H_REG     BK_REG(0x35)

#define TIMER_TRIG_BIT       BIT_1
#define TIMER_EN_BIT         BIT_0
#define TIMER_INT_EN_BIT     BIT_8
#define TIMER_HIT_BIT        BIT_0

#define MOTO_V_GPIO_1 401
#define MOTO_V_GPIO_2 400
#define MOTO_V_GPIO_3 399
#define MOTO_V_GPIO_4 398
#define MOTO_H_GPIO_1 509
#define MOTO_H_GPIO_2 508
#define MOTO_H_GPIO_3 498
#define MOTO_H_GPIO_4 498


#define PTZ_ERROR_CMD_INVALID (-1)
#define PTZ_ERROR_MOTOR_CTL_INVALID (-2)
#define PTZ_ERROR_MOTOR_STOP_INVALID (-3)
#define PTZ_ERROR_IRCUT_CTL_INVALID (-4)
#define PTZ_ERROR_TIMER_CTL_INVALID (-5)
#define PTZ_ERROR_OTHER (-99)

enum
{
    PTZ_CTL_MOTOR_UP = 10,
    PTZ_CTL_MOTOR_DOWN,
    PTZ_CTL_MOTOR_LEFT,
    PTZ_CTL_MOTOR_RIGHT,
    PTZ_CTL_MOTOR_STOP,
    PTZ_CTL_REMAIN_STEP,
    PTZ_CTL_TIMER_TIME,
    PTZ_CTL_DEBUG,
    PTZ_CTL_NULL,
};

typedef enum PTZ_CTL_STATUS
{
    CTL_STATUS_NONE,
    CTL_STATUS_MOTOR_FORWORD_H,
    CTL_STATUS_MOTOR_BACKWORD_H,
    CTL_STATUS_MOTOR_FORWORD_V,
    CTL_STATUS_MOTOR_BACKWORD_V,
    CTL_STATUS_MOTOR_STOP,
    CTL_STATUS_DONE,
} PTZ_CTL_STATUS;

typedef struct
{
    volatile PTZ_CTL_STATUS m_status;
    volatile int m_step;
    volatile int m_time;
    volatile int m_bInterrupt;
} ptz_ctl_s;

static int virq = -1;
static struct class *sys_class = NULL;
static dev_t ptz_devno;
static spinlock_t my_lock;

static int gstPtzForwordH[MAX_MOTOR_CNT] = {0x01, 0x03, 0x02, 0x06, 0x04, 0x0c, 0x08, 0x09};
static int gstPtzBackwordH[MAX_MOTOR_CNT] = {0x01, 0x09, 0x08, 0x0c, 0x04, 0x06, 0x02, 0x03};
static int gstPtzForwordV[MAX_MOTOR_CNT] = {0x10, 0x30, 0x20, 0x60, 0x40, 0xc0, 0x80, 0x90};
static int gstPtzBackwordV[MAX_MOTOR_CNT] = {0x10, 0x90, 0x80, 0xc0, 0x40, 0x60, 0x20, 0x30};

static ptz_ctl_s gstPtz;
static volatile int gstPtzCnt = 0;
static unsigned int time1_L = 0;
static unsigned int time1_H = 0;
static unsigned long long int time0_count = 0;

static void ptz_send_data(int data, int Direction)
{
    if (Direction)
    {
        gpio_set_value(MOTO_V_GPIO_1, (data & 0x01));
        gpio_set_value(MOTO_V_GPIO_2, ((data >> 1) & 0x01));
        gpio_set_value(MOTO_V_GPIO_3, ((data >> 2) & 0x01));
        gpio_set_value(MOTO_V_GPIO_4, ((data >> 3) & 0x01));
    }
    else
    {
        gpio_set_value(MOTO_H_GPIO_1, (data & 0x01));
        gpio_set_value(MOTO_H_GPIO_2, ((data >> 1) & 0x01));
        gpio_set_value(MOTO_H_GPIO_3, ((data >> 2) & 0x01));
        gpio_set_value(MOTO_H_GPIO_4, ((data >> 3) & 0x01));
    }
}

static void ptz_motor_forword_h(void)
{
    if (gstPtz.m_step > 0)
    {
        if (gstPtzCnt < MAX_MOTOR_CNT)
        {
            ptz_send_data(gstPtzForwordH[gstPtzCnt], 0);
            gstPtzCnt++;
        }
        if (gstPtzCnt >= MAX_MOTOR_CNT)
        {
            gstPtz.m_step--;
            gstPtzCnt = 0;

            /*中断电机操作*/
            if (gstPtz.m_bInterrupt)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
            }
        }
    }
    else
    {
        gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
    }
}

static void ptz_motor_backword_h(void)
{
    if (gstPtz.m_step > 0)
    {
        if (gstPtzCnt < MAX_MOTOR_CNT)
        {
            ptz_send_data(gstPtzBackwordH[gstPtzCnt], 0);
            gstPtzCnt++;
        }
        if (gstPtzCnt >= MAX_MOTOR_CNT)
        {
            gstPtz.m_step--;
            gstPtzCnt = 0;

            /*中断电机操作*/
            if (gstPtz.m_bInterrupt)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
            }
        }
    }
    else
    {
        gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
    }
}

static void ptz_motor_forword_v(void)
{
    if (gstPtz.m_step > 0)
    {
        if (gstPtzCnt < MAX_MOTOR_CNT)
        {
            ptz_send_data(gstPtzForwordV[gstPtzCnt], 1);
            gstPtzCnt++;
        }
        if (gstPtzCnt >= MAX_MOTOR_CNT)
        {
            gstPtz.m_step--;
            gstPtzCnt = 0;

            /*中断电机操作*/
            if (gstPtz.m_bInterrupt)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
            }
        }
    }
    if (gstPtz.m_step <= 0)
    {
        gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
    }
}

static void ptz_motor_backword_v(void)
{
    if (gstPtz.m_step > 0)
    {
        if (gstPtzCnt < MAX_MOTOR_CNT)
        {
            ptz_send_data(gstPtzBackwordV[gstPtzCnt], 1);
            gstPtzCnt++;
        }
        if (gstPtzCnt >= MAX_MOTOR_CNT)
        {
            gstPtz.m_step--;
            gstPtzCnt = 0;

            /*中断电机操作*/
            if (gstPtz.m_bInterrupt)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
            }
        }
    }
    if (gstPtz.m_step <= 0)
    {
        gstPtz.m_status = CTL_STATUS_MOTOR_STOP;
    }
}

static void ptz_stop(void)
{
    gstPtz.m_status = CTL_STATUS_DONE;
}

static void ptz_done(void)
{
    gpio_set_value(MOTO_V_GPIO_1, 0);
    gpio_set_value(MOTO_V_GPIO_2, 0);
    gpio_set_value(MOTO_V_GPIO_3, 0);
    gpio_set_value(MOTO_V_GPIO_4, 0);
    gpio_set_value(MOTO_H_GPIO_1, 0);
    gpio_set_value(MOTO_H_GPIO_2, 0);
    gpio_set_value(MOTO_H_GPIO_3, 0);
    gpio_set_value(MOTO_H_GPIO_4, 0);
    gstPtz.m_status = CTL_STATUS_NONE;
    gstPtz.m_bInterrupt = 0;
}

irqreturn_t ptz_int_service(int irq, void *dummy)
{
    if (time0_count >= gstPtz.m_time)
    {
        time0_count = 0;
        switch (gstPtz.m_status)
        {
        case CTL_STATUS_MOTOR_FORWORD_H:
        {
            ptz_motor_forword_h();
            break;
        }
        case CTL_STATUS_MOTOR_BACKWORD_H:
        {

            ptz_motor_backword_h();
            break;
        }
        case CTL_STATUS_MOTOR_FORWORD_V:
        {

            ptz_motor_forword_v();
            break;
        }
        case CTL_STATUS_MOTOR_BACKWORD_V:
        {

            ptz_motor_backword_v();
            break;
        }
        case CTL_STATUS_MOTOR_STOP:
        {

            ptz_stop();
            break;
        }
        case CTL_STATUS_DONE:
        {

            ptz_done();
            break;
        }
        default:
            break;
        }
    }

    time0_count++;
#if 0
    if(time0_count == (1000 * 100)){
        time0_count = 0;
        //printk("[%d]ptz_int_service doing\n\n", irq);//开启kmsg看log，1s 打印一次
    }
#endif
    return IRQ_HANDLED;
}

static long ptz_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int iRet = 0;
    spin_lock(&my_lock);
    printk("cmd:%d step:%ld\n", cmd, arg);
    printk("sta:%d,time:%d,step:%d,inter:%d\n", gstPtz.m_status, gstPtz.m_time, gstPtz.m_step, gstPtz.m_bInterrupt);
    switch (cmd)
    {
    case PTZ_CTL_DEBUG:
    {
        gpio_set_value(MOTO_V_GPIO_1, 1);
        gpio_set_value(MOTO_V_GPIO_2, 1);
        gpio_set_value(MOTO_V_GPIO_3, 1);
        gpio_set_value(MOTO_V_GPIO_4, 1);
        gpio_set_value(MOTO_H_GPIO_1, 1);
        gpio_set_value(MOTO_H_GPIO_2, 1);
        gpio_set_value(MOTO_H_GPIO_3, 1);
        gpio_set_value(MOTO_H_GPIO_4, 1);
        printk("sta:%d,time:%d,step:%d, inter:%d\n",
               gstPtz.m_status, gstPtz.m_time, gstPtz.m_step, gstPtz.m_bInterrupt);
        break;
    }
    case PTZ_CTL_TIMER_TIME:

    {
        if (CTL_STATUS_NONE == gstPtz.m_status)
        {
            gstPtz.m_time = arg;
        }
        else
        {
            printk("Invalid cmd:%d,sta:%d,step:%d\n", cmd, gstPtz.m_status, gstPtz.m_step);
            iRet = PTZ_ERROR_TIMER_CTL_INVALID;
        }
        break;
    }
    case PTZ_CTL_MOTOR_UP:
    case PTZ_CTL_MOTOR_LEFT:
    {
        /*确保step返回应用层，才能继续控电机*/
        if ((gstPtz.m_step == 0) &&
            (CTL_STATUS_NONE == gstPtz.m_status))
        {
            gstPtz.m_step = arg;
            if (PTZ_CTL_MOTOR_UP == cmd)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_FORWORD_V;
            }
            else
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_FORWORD_H;
            }
        }
        else
        {
            printk("Invalid cmd:%d,sta:%d,step:%d\n", cmd, gstPtz.m_status, gstPtz.m_step);
            iRet = PTZ_ERROR_MOTOR_CTL_INVALID;
        }
        break;
    }
    case PTZ_CTL_MOTOR_DOWN:
    case PTZ_CTL_MOTOR_RIGHT:
    {
        if ((gstPtz.m_step == 0) &&
            (CTL_STATUS_NONE == gstPtz.m_status))
        {
            gstPtz.m_step = arg;
            if (PTZ_CTL_MOTOR_DOWN == cmd)
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_BACKWORD_V;
            }
            else
            {
                gstPtz.m_status = CTL_STATUS_MOTOR_BACKWORD_H;
            }
        }
        else
        {
            printk("Invalid cmd:%d, sta:%d,step:%d\n", cmd, gstPtz.m_status, gstPtz.m_step);
            iRet = PTZ_ERROR_MOTOR_CTL_INVALID;
        }
        break;
    }
    case PTZ_CTL_MOTOR_STOP:
    {
        if ((CTL_STATUS_MOTOR_FORWORD_H <= gstPtz.m_status) && (CTL_STATUS_MOTOR_BACKWORD_V >= gstPtz.m_status))
        {
            gstPtz.m_bInterrupt = 1;
            iRet = PTZ_ERROR_MOTOR_STOP_INVALID;
        }
        else if (CTL_STATUS_NONE == gstPtz.m_status)
        {
            iRet = gstPtz.m_step;
            gstPtz.m_step = 0;
        }
        else
        {
            iRet = PTZ_ERROR_MOTOR_STOP_INVALID;
        }
        printk("PTZ_CTL_MOTOR_STOP, sta:%d,step:%d, iRet:%d\n", gstPtz.m_status, gstPtz.m_step, iRet);
        break;
    }
    case PTZ_CTL_REMAIN_STEP:
    {
        iRet = gstPtz.m_step;
        if (iRet < 0)
        {
            printk("Invalid iStep %d\n", iRet);
            iRet = 0;
        }
        break;
    }
    default:
    {
        printk("[ptz ctrl]: get error cmd %d\n", cmd);
        iRet = PTZ_ERROR_CMD_INVALID;
        break;
    }
    }
    spin_unlock(&my_lock);
    printk("iRet:%d\n", iRet);
    return iRet;
}

static int ptz_gpio_init(void)
{
    int iRet = 0;
    const char *desc = "mdrv_gpioirq";
    iRet = gpio_request(MOTO_V_GPIO_1, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_1, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_V_GPIO_2, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_2, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_V_GPIO_3, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_3, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_V_GPIO_4, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_4, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_H_GPIO_1, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_1, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_H_GPIO_2, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_2, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_H_GPIO_3, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_3, iRet);
        return iRet;
    }
    iRet = gpio_request(MOTO_H_GPIO_4, desc);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_4, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_V_GPIO_1, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_1, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_V_GPIO_2, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_2, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_V_GPIO_3, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_3, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_V_GPIO_4, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_V_GPIO_4, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_H_GPIO_1, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_1, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_H_GPIO_2, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_2, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_H_GPIO_3, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_3, iRet);
        return iRet;
    }
    iRet = gpio_direction_output(MOTO_H_GPIO_4, 0);
    if (iRet < 0)
    {
        printk("failed to configure direction for GPIO %d, error %d\n", MOTO_H_GPIO_4, iRet);
        return iRet;
    }

    gpio_set_value(MOTO_V_GPIO_1, 0);
    gpio_set_value(MOTO_V_GPIO_2, 0);
    gpio_set_value(MOTO_V_GPIO_3, 0);
    gpio_set_value(MOTO_V_GPIO_4, 0);
    gpio_set_value(MOTO_H_GPIO_1, 0);
    gpio_set_value(MOTO_H_GPIO_2, 0);
    gpio_set_value(MOTO_H_GPIO_3, 0);
    gpio_set_value(MOTO_H_GPIO_4, 0);
    return iRet;
}

static void ptz_gpio_uninit(void)
{
    gpio_free(MOTO_V_GPIO_1);
    gpio_free(MOTO_V_GPIO_2);
    gpio_free(MOTO_V_GPIO_3);
    gpio_free(MOTO_V_GPIO_4);
    gpio_free(MOTO_H_GPIO_1);
    gpio_free(MOTO_H_GPIO_2);
    gpio_free(MOTO_H_GPIO_3);
    gpio_free(MOTO_H_GPIO_4);
}

static int ptz_ctl_open(struct inode *inode, struct file *filp)
{
    int minor = MINOR(inode->i_rdev);

    if (minor >= MAX_PTZ_MINORS)
        return -ENODEV;

    filp->private_data = (void *)minor;

    return 0;
}

static int ptz_ctl_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations ptz_ctl_fops = {
    .unlocked_ioctl = ptz_ioctl,
    .open = ptz_ctl_open,
    .release = ptz_ctl_release,
    .owner = THIS_MODULE,
};

static struct cdev ptz_ctl_cdev = {
    .kobj = {
        .name = "APP_PTZ",
    },
    .owner = THIS_MODULE,
};

static int ptz_ctl_init(void)
{
    printk("[ptz ctrl] init start 2025-10-25\n");
    ptz_devno = MKDEV(PTZ_MAJOR, 0);

    if (register_chrdev_region(ptz_devno, MAX_PTZ_MINORS, "APP_PTZ"))
    {
        return 0;
    }

    cdev_init(&ptz_ctl_cdev, &ptz_ctl_fops);
    if (cdev_add(&ptz_ctl_cdev, ptz_devno, MAX_PTZ_MINORS))
    {
        kobject_put(&ptz_ctl_cdev.kobj);
        unregister_chrdev_region(ptz_devno, MAX_PTZ_MINORS);
        return 0;
    }

    sys_class = class_create(THIS_MODULE, PTZ_DEV_NAME);
    device_create(sys_class, NULL, ptz_devno, NULL, PTZ_DEV_NAME);

    memset(&gstPtz, 0, sizeof(ptz_ctl_s));
    printk("sta:%d,time:%d,step:%d,inter:%d\n", gstPtz.m_status, gstPtz.m_time, gstPtz.m_step, gstPtz.m_bInterrupt);

    printk("[ts ptz ctrl] init success.\n");

    return 0;
}

static void ptz_ctl_exit(void)
{
    device_destroy(sys_class, ptz_devno);
    class_destroy(sys_class);

    cdev_del(&ptz_ctl_cdev);
    unregister_chrdev_region(ptz_devno, MAX_PTZ_MINORS);
    ptz_gpio_uninit();

    return;
}

static int ptz_timer_probe(struct platform_device *pdev)
{
    ptz_ctl_init();
    ptz_gpio_init();

    {
        S32 s32_ret = -1;
        struct device_node *intr_node;
        struct irq_domain *intr_domain;
        struct irq_fwspec fwspec;
        intr_node = of_find_compatible_node(NULL, NULL, "sstar,main-intc");
        intr_domain = irq_find_host(intr_node);
        if (!intr_domain)
        {
            printk("timer_mod : irq_find_host fail \n");
            return -ENXIO;
        }

        fwspec.param_count = 3;
        fwspec.param[0] = GIC_SPI;
        fwspec.param[1] = INT_FIQ_TIMER_1;
        fwspec.param[2] = IRQ_TYPE_NONE;
        fwspec.fwnode = of_node_to_fwnode(intr_node);
        virq = irq_create_fwspec_mapping(&fwspec);
        s32_ret = request_irq(virq, ptz_int_service, IRQF_SHARED, "ptz_int_service", (void *)ptz_int_service);
        if (s32_ret)
        {
            printk("timer_modules Err: request_irq fail %d \n", s32_ret);
        }

        {
            unsigned int timer = (TIMER_US_HZ);
            time1_L = timer & 0xFFFF;
            time1_H = (timer & 0xFFFF0000) >> 16;
            // printk("timer0timer:0x%x  0x%x\n",time1_L, time1_H);

            OUTREG16(BASE_REG_TIMER_PA + TIMER1_EN_REG, 0x0);        // 清除
            OUTREG16(BASE_REG_TIMER_PA + TIMER1_MAX_L_REG, time1_L); // set 32bits counter
            OUTREG16(BASE_REG_TIMER_PA + TIMER1_MAX_H_REG, time1_H);
            SETREG16(BASE_REG_TIMER_PA + TIMER1_EN_REG, TIMER_INT_EN_BIT | 0x1); // Enable interrupt & triger oneshot
        }
    }

    return 0;
}

static int ptz_timer_remove(struct platform_device *dev)
{
    free_irq(virq, ptz_int_service);
    ptz_ctl_exit();
    printk("[Timer]ptz_timer_remove \n");
    return 0;
}

static void ptz_timer_shutdown(struct platform_device *dev)
{
    printk("[Timer]ptz_timer_shutdown \n");
}

static const struct of_device_id ms_timer_of_match_table[] = {
    {.compatible = "sstar,timer"}, // arm,armv7-timer   sstar,infinity-timer
    {}};
MODULE_DEVICE_TABLE(of, ms_timer_of_match_table);

static struct platform_driver ptz_timer_driver = {
    .probe = ptz_timer_probe,
    .remove = ptz_timer_remove,
    .shutdown = ptz_timer_shutdown,

    .driver = {
        .owner = THIS_MODULE,
        .name = "ptz_timer",
        .of_match_table = ms_timer_of_match_table,
    },
};

module_platform_driver(ptz_timer_driver);

MODULE_AUTHOR("leo.lin");
MODULE_DESCRIPTION("ptz timer Device Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:infinity-timer");
