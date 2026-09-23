#ifndef __ANJ_MCU_H__
#define __ANJ_MCU_H__


#define MCU_UART_DEV            "/dev/ttyS2"           // 串口设备路径
#define MCU_VER_TMP_PATH        "/tmp/mcu_ver.txt"

#define MCU_VER_PATH            "/mnt/nand/mcu_ver.txt"
#define MCU_AOV_BIN_NEW_PATH    "/mnt/nand/aov_mcu.bin"
#define MCU_AOV_BIN_ORG_PATH    "/opt/ch/aov_mcu.bin"

#define MCU_RESET_FILE          "/mnt/nand/reset_mcu_flag"

typedef enum tagE_AOVCmd
{
	E_AOVCmd_GetMcuVer = 0xA0,       //获取MCU版本号
	E_AOVCmd_GetWakeupSrc,		     //获取唤醒源
	E_AOVCmd_SetWakeupSrc,		     //设置唤醒源
	E_AOVCmd_GetWakeupTimerInvertal, //获取定时唤醒时间间隔
	E_AOVCmd_SetWakeupTimerInvertal, //设置定时唤醒时间间隔
	E_AOVCmd_LampCtrl,				 //红外白光控制
	E_AOVCmd_RstNotify,				 //复位通知
	E_AOVCmd_BatteryLevelNotify,	 //电量通知
	E_AOVCmd_GetDelayPowerDownTime,  //获取MCU延时掉电时间
	E_AOVCmd_SetDelayPowerDownTime,  //设置MCU延时掉电时间
	E_AOVCmd_GetStrFlag,			 //获取启动标识 IPL用
	E_AOVCmd_SetStrFlag,			 //设置启动标识 IPL用
	E_AOVCmd_RBLedCtrl,			 	 //红蓝警灯控制
	E_AOVCmd_GetMcuRunTime,			 //获取mcu运行时间, 暂时没用到
	E_AOVCmd_SetHbInterval,		 	 //设置心跳检查间隔
	E_AOVCmd_UpgrateNotify,			 //升级通知
	E_AOVCmd_End
}E_AOVCmd;


typedef enum tagE_WakeupSrc
{
	E_WakeupSrc_AlwaysOn = 0,       //长电，退出休眠
	E_WakeupSrc_Timer,		        //定时器
	E_WakeupSrc_Net,		        //网络唤醒
	E_WakeupSrc_Empty		        //空，用来初始化
}E_WakeupSrc;

typedef enum tagE_LampType
{
	E_LampType_WhiteLed = 0,  //白光
	E_LampType_IrLed         //红外
}E_LampType;

typedef enum tagE_RBLedState
{
	E_RBLedState_OFF = 0,  //开启
	E_RBLedState_ON        //关闭
}E_RBLedState;

typedef struct
{
	int nInitFlag;                  // 初始化标志
	int iRuningFlag;                // 运行标志
	int fd;                         //串口句柄
	int nWakeupTimerInvervalMs;     //mcu定时器间隔
	int nDelayPowerDownTimeMs;      //mcu收到唤醒脚变化，延时下电时间
	int nHbIntervals;               //心跳间隔，默认60s

	E_WakeupSrc eWakeupSrc;         //唤醒源
	E_LampType eLampType;           //灯状态
	unsigned int nPwmValue;         //灯亮度
	E_RBLedState eRBLedState;
	
	char acMcuVer[16];              //mcu版本号
	
	unsigned char getMcuVerRsp;                 //获取版本号应答是否收到应答
	unsigned char setWakeupSrcRsp;              //设置唤醒源是否收到应答
	unsigned char getWakeupSrcRsp;              //设置唤醒源是否收到应答
	unsigned char setWakeupTimerIntervalRsp;    //设置唤醒定时器间隔是否收到应答
	unsigned char setHbIntervalRsp;             //设置唤醒定时器间隔是否收到应答

	int nNeedUpdateFlag;
	char UpdateCurVerInfo[64];
}AnjMcuCtrlInfo_t;


// mcu模块和aov模块高度耦合，暂不用eventhub的方式调用，而是直接调用api接口
int anj_mcu_set_power_down_delay_ms(unsigned int nDelayTimeMs);
int anj_mcu_set_heartbeat_interval(unsigned int nHbIntervals);




#endif
