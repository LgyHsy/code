#ifndef __ANJ_GYRO_H__
#define __ANJ_GYRO_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int ready;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float accel_x;
    float accel_y;
    float accel_z;
    float temp;
} anj_gyro_data_t;

#ifdef __cplusplus
}
#endif

#endif
