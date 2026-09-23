#ifndef __ANJ_PTZIIC_GC615_H__
#define __ANJ_PTZIIC_GC615_H__

#include <linux/types.h>

#define GC615_MOTOR_A (0) /* A 通道，接水平电机 */
#define GC615_MOTOR_B (1) /* B 通道，接垂直电机 */

#define GC615_DIR_FORWARD (0)
#define GC615_DIR_REVERSE (1)

/* IRCUT 线圈翻转维持时间，之后必须进刹车 */
#define GC615_IRCUT_HOLD_MS (200)

int gc615_config(u8 chip_addr);
u8 gc615_chip_id(void);

int gc615_motor_run(int motor, int dir, u32 period, u32 pulses);
int gc615_motor_stop(int motor);
int gc615_steps_clear(int motor);
u32 gc615_steps_read(int motor);

/* 只发起翻转，调用方需在锁外等 GC615_IRCUT_HOLD_MS 后再调 gc615_ircut_brake */
int gc615_ircut_drive(int day);
int gc615_ircut_brake(void);

int gc615_output_disable(void);

#endif /* __ANJ_PTZIIC_GC615_H__ */
