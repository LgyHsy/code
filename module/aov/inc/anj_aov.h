#ifndef __ANJ_AOV_H__
#define __ANJ_AOV_H__


typedef enum tagE_FpsType
{
	E_FpsType_Low = 0,
	E_FpsType_High
}E_FpsType;

typedef enum tagE_BatteryLevel
{
	E_BatteryLevel_High =0,
	E_BatteryLevel_Low
}E_BatteryLevel;

typedef enum tagE_RemoteStatus
{
	E_RemoteStatus_Connect = 0,
	E_RemoteStatus_DisConn
}E_RemoteStatus;

typedef enum tagE_DetResult
{
	E_DetResult_Detected = 0,
	E_DetResult_UnDetected
}E_DetResult;

typedef enum tagE_DevRunStat
{
    E_RUN_STAT_NORMAL = 0,      // 常电模式
    E_RUN_STAT_AOV              // AOV模式
}E_DevRunStat;

typedef enum tagE_DevUsrSetMode
{
    E_USR_AOV_MODE_NORMAL = 0,      // 长电模式
    E_USR_AOV_MODE_LOWBAT,          // 微电模式
    E_USR_AOV_MODE_AOV              // AOV模式
}E_DevUsrAOVMode;

enum
{
    E_USR_AOV_PER_FRAME_SEC_1 = 1,      // 1s1帧
    E_USR_AOV_PER_FRAME_SEC_2 = 2,      // 2s1帧
    E_USR_AOV_PER_FRAME_SEC_4 = 4       // 4s1帧
};

typedef enum 
{
    CHARGING_OFF = 0,
    CHARGING_ON
}E_ChargeStat;

typedef enum
{
    E_WAKEUP_EVENT_POWER_ON = 0,        // 上电
    E_WAKEUP_EVENT_NET,                 // 远程网络唤醒
    E_WAKEUP_EVENT_ALGO_HUMAN,          // 人形唤醒
    E_WAKEUP_EVENT_KEY_RST              // 复位按键唤醒
}E_WakeUpEvent;


typedef enum
{
    E_MCU_WAKEUP_SRC_ALWAYS_ON = 0,
    E_MCU_WAKEUP_SRC_TIMER,
    E_MCU_WAKEUP_SRC_NET
}E_McuWakeUpSrc;



typedef struct
{
    int iUserWorkMode;        // 用户设置模式 0-长电模式 1-微电模式 2-AOV模式
    int iUserAovFps;          // 用户设置AOV帧数 支持1s1帧 2s1帧 4s一帧，默认按1s1帧

    E_DevRunStat eCurRunStat;           // 当前运行状态：0-常电 1-AOV

    E_FpsType eCurFpsType;              // 当前帧率类型
    E_FpsType eLastFpsType;

    E_WakeUpEvent eCurWakeUpEvent;      // 当前唤醒设备事件
    E_McuWakeUpSrc eWakeupSrcToMcu;                // 设置到mcu的唤醒事件
    E_McuWakeUpSrc eLastWakeupSrcToMcu;

    E_BatteryLevel eBatteryLevel;       // 电量状态
    E_ChargeStat iChargeStatus;         // 充电状态
    int iBatteryCapacity;               // 当前电量

    E_DetResult eCurDetResult;              // 当前检测结果
    unsigned long long uHumanDetectTime;    // 人形检测时间

    struct timeval stResumeTime;
}AnjAovCtrlInfo_t;

#endif