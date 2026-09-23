#ifndef __ANJ_PTZ_H__
#define __ANJ_PTZ_H__

#include "anj_config.h"
#include "anj_config_ptz.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PTZ_ERROR_CMD_INVALID (-1)
#define PTZ_ERROR_MOTOR_CTL_INVALID (-2)
#define PTZ_ERROR_MOTOR_STOP_INVALID (-3)
#define PTZ_ERROR_IRCUT_CTL_INVALID (-4)
#define PTZ_ERROR_TIMER_CTL_INVALID (-5)
#define PTZ_ERROR_OTHER (-99)

#define PTZ_WAIT_TIME (10 * 1000)                              // 10ms
#define PTZ_STOP_OVER_TIME (1000 * 1000 / PTZ_WAIT_TIME)       // 1s over time
#define PTZ_RUN_OVER_TIME ((60 * 1000 * 1000) / PTZ_WAIT_TIME) // 1min over time

enum
{
    PTZ_NONE = 0,
    PTZ_STOP,
    PTZ_UP,
    PTZ_DOWN,
    PTZ_LEFT,
    PTZ_RIGHT,
    PTZ_RESET,
    PTZ_PRESET,
    PTZ_GUARD,
    PTZ_TRACK,
    PTZ_LINE_SCAN,
    PTZ_CRUISE,
    PTZ_3D_POSITION,
    PTZ_LENS_COVER_ON,
    PTZ_LENS_COVER_OFF,
    PTZ_ZOOM,
};

enum
{
    PTZ_SPEED_1 = 1,
    PTZ_SPEED_2,
    PTZ_SPEED_3,
    PTZ_SPEED_4,
    PTZ_SPEED_5,
    PTZ_SPEED_6,
    PTZ_SPEED_7,
    PTZ_SPEED_8,
    PTZ_SPEED_9,
    PTZ_SPEED_10,
};

enum
{
    PTZ_CTL_MOTOR_UP = 10, /*设置主电机向上转动*/
    PTZ_CTL_MOTOR_DOWN,    /*设置主电机向下转动*/
    PTZ_CTL_MOTOR_LEFT,    /*设置主电机向左转动*/
    PTZ_CTL_MOTOR_RIGHT,   /*设置主电机向右转动*/
    PTZ_CTL_MOTOR_STOP,    /*设置电机停止工作*/
    PTZ_CTL_REMAIN_STEP,   /*获取电机转动的步数*/
    PTZ_CTL_SPEED_SET,     /*设置电机转动定时器速度*/
    PTZ_CTL_HDIR_SET,      /*设置电机水平转动方向*/
    PTZ_CTL_VDIR_SET,      /*设置电机垂直转动方向*/
    PTZ_CTL_DEBUG,
    PTZ_CTL_NULL,
};

typedef struct stPTZCfg
{
    int bUpdate;                /*记录人形数据是否更新*/
    int bTrackStart;
    PD_AREA_ENTRY ptzTrackArea;   /*记录人形区域及坐标*/

    unsigned long long startTrackTime;
    unsigned long long endTrackTime;
} PtzTrack;

typedef struct stPTZLineScanRuntime
{
    int scanDir;
} PtzLineScanRuntime;

typedef struct stPTZCruiseRuntime
{
    int nextPresetId;
    unsigned long long startTime;
} PtzCruiseRuntime;

typedef struct ptz_param
{
    unsigned char preset_id;        /*预置点号*/
    int ptzWork;                    /*云台工作模式(追踪 复位 预置点)*/
    PtzStep m_PtzStep;              /*当前位置的步数*/
    int bptzInterrupt;              /*是否对云台进行打断操作*/
    int bInitCheck;                 /*是否自检*/
    PtzStep ptzMisTakeStep;         /*机壳误差步数*/
    unsigned long long ptzNoneTime; /*云台无操作的时间*/
    unsigned long long ptzOsdTime;  /*云台OSD显示时间*/
    PtzTrack ptzTrack;              /*云台追踪配置参数*/
    PtzLineScanRuntime ptzLineScan; /*云台线扫运行态*/
    PtzCruiseRuntime ptzCruise;     /*云台巡航运行态*/
    PtzStep ptz3DStep;              /*云台3D定位步数*/
    PtzStep MaxStep;                /*记录云台可运行的最大步数*/
    int ptzVStepAddup;              /*追踪过程垂直步数累加*/
    int ptzBurnInTime;              /*老化云台间隔标记*/
    int bLensCoverEnable;           /*镜头遮挡使能*/
    int bLensCoverStepValid;        /*镜头遮挡前位置是否有效*/
    PtzStep lensCoverStep;          /*镜头遮挡前位置*/
    double ptzZoomMultiple;         /*当前手动变倍目标倍率*/
} PTZ_PARAM;

void anj_ptz_work_update(int work);

#ifdef __cplusplus
}
#endif

#endif