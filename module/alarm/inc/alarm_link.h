#ifndef __ALARM_LINK_H__
#define __ALARM_LINK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define FILE_PLAY_PATH_MAX_LEN 64

#define ALARM_OUT_MAX_NUM       4

// 报警事件开始和结束
#define ALARM_FLAG_OCCUR				1
#define ALARM_FLAG_DISAPPEAR			0

#define ALARM_LEVEL_EVENT				1
#define ALARM_LEVEL_ERROR				2
#define ALARM_LEVEL_SERIOUS				3
#define ALARM_LEVEL_EMERGENCY			4
#define ALARM_LEVEL_DISASTER			5

typedef enum {
	ALARM_CODE_BEGIN=0,
	ALARM_CODE_LINKDOWN=1,
	ALARM_CODE_LINKUP = 2,
	ALARM_CODE_USB_PLUG = 3,
	ALARM_CODE_USB_UNPLUG = 4,
	ALARM_CODE_SD0_PLUG = 5,
	ALARM_CODE_SD0_UNPLUG = 6,
	ALARM_CODE_SD1_PLUG = 7,
	ALARM_CODE_SD1_UNPLUG = 8,
	ALARM_CODE_USB_FREESPACE_LOW = 9,
	ALARM_CODE_SD0_FREESPACE_LOW = 10,
	ALARM_CODE_SD1_FREESPACE_LOW = 11,
	ALARM_CODE_VIDEO_LOST = 12,
	ALARM_CODE_VIDEO_COVERD = 13,
	ALARM_CODE_MOTION_DETECT = 14,
	ALARM_CODE_GPIO3_HIGH2LOW = 15,	            //仅仅用于告警触发判断。IO报警使用ALARM_CODE_IO_ALARM和ALARM_CODE_IO_ALARM_FINISH
	ALARM_CODE_GPIO3_LOW2HIGH = 16,	            //仅仅用于告警触发判断。IO报警使用ALARM_CODE_IO_ALARM和ALARM_CODE_IO_ALARM_FINISH
	ALARM_CODE_STORAGE_FREESPACE_LOW = 17, 
	ALARM_CODE_RECORD_START = 18,
	ALARM_CODE_RECORD_FINISHED = 19,	
	ALARM_CODE_RECORD_FAILED = 20,	
	ALARM_CODE_VIDEO_AI = 21,	        //智能分析报警，包含人形检测、车型检测、电动车、车牌、火焰等等，使用AlarmLevel字段作为子类型，取值参考AjAiAlarmType
	ALARM_CODE_VIDEO_AI_FINISH = 22,    //智能分析报警消除，使用AlarmLevel作为子类型
	ALARM_CODE_JPEG_CAPTURED = 23,	
	ALARM_CODE_RS485_DATA = 24,		
	ALARM_CODE_SAME_IP = 25,			
	ALARM_CODE_HW130_PIR = 26,
	ALARM_CODE_LPR = 27,	            //车牌识别
	ALARM_CODE_AUDIO_BABYCRY = 28,      //婴儿啼哭
	ALARM_CODE_AUDIO_LSA = 29,          //高分贝声音

	ALARM_CODE_VIDEO_FORMAT_CHANGED = 30,	//格式/分辨率更改，用于通知客户端重新配置解码器

	ALARM_CODE_VIDEO_GATE = 31,                 //电子围栏

	ALARM_CODE_RESET_TO_FACTORY = 32,           //恢复出厂通知
	ALARM_CODE_MOTION_DETECT_DISAPPEAR = 33,    //移动侦测告警消除
	
	ALARM_CODE_IO_ALARM = 34,	        //IO输入报警,用于一直按下的情况下，就一直告警
	ALARM_CODE_IO_ALARM_FINISH = 35,	//IO输入报警结束
	ALARM_CODE_GPS_INFO = 36,			
	ALARM_CODE_EMERGENCY_CALL = 37,
	ALARM_CODE_VIDEO_GATE_FINISH = 38,

	ALARM_CODE_CONFIG_CHANGED = 39,     //用于通知devsdk/配置更改
	ALARM_CODE_BEGIN_REBOOT = 40,	    //准备重启
	ALARM_CODE_TEMP_HUMID_ALARM = 41,   //温湿度告警
	
	ALARM_CODE_EXTERNAL_IO_ALARM = 42,
	ALARM_CODE_EXTERNAL_IO_ALARM_FINISH = 43, 

	ALARM_CODE_KEY_PRESS = 44,
	ALARM_CODE_SENSOR = 45,                     //传感器报警，使用AlarmSensor作为子类型
	ALARM_CODE_PTZSTATUS = 46,                  //云台状态变化
	ALARM_CODE_FILE_READY_FOR_DOWNLOAD = 47,    //文件已经可以下载
	
	ALARM_CODE_CALL=150,                        //VOIP 呼叫相关，
	ALARM_CODE_CALL_CALLOUT = 151,              //callout 呼叫
	ALARM_CODE_CALL_THROUGH = 152,              //through 接通
	ALARM_CODE_CALL_HANGUP = 153,               //hangup 挂断
	ALARM_CODE_CALL_TOHANGUP = 154,             //timeout_hangup 超时挂断

	ALARM_CODE_ILLEGAL_MODIFY = 200,            //非法修改操作
	ALARM_CODE_CUSTOM_INFO,                     //自定义信息，通过notes携带
	ALARM_CODE_SD_FORMAT_FAIL,
	ALARM_CODE_SD_FORMAT_FINISH,

	ALARM_CODE_END
}AjAlarmCode;


typedef enum
{
	ALARM_SENSOR_OFF = 0,           //无报警或报警消除
	ALARM_SENSOR_ON  = 1,           //常规报警
	ALARM_SENSOR_POW_CUT  = 2,      //停电报警
	ALARM_SENSOR_OUT_OF_SCOPE  = 3, //传感器超范围报警
	ALARM_SENSOR_FAULT  = 4,        //设备故障
	ALARM_SENSOR_LOWPOWER = 5,      //电量低
	ALARM_SENSOR_MAX
}AjSensorAlarmType;


typedef enum
{
	ALARM_AI_PD = 1,                        //人形
	ALARM_AI_VEHICLE_CAR = 2,               //车形
	ALARM_AI_VEHICLE_MOTO = 3,              //摩托
	ALARM_AI_VEHICLE_ELECTRICBICYCLE = 4,   //电单车
	ALARM_AI_VEHICLE_BICYCLE = 5,           //自行车
	ALARM_AI_LPR = 6,               //车牌
	ALARM_AI_VIDEO_GATE = 7,        //越界(拌线)
	ALARM_AI_FIRE = 8,	            //火焰
	ALARM_AI_FACEDETECT = 9,//FACE DETECT
	ALARM_AI_VIDEO_REGION_DETECT_ENTER = 10,//区域侦测进入
	ALARM_AI_VIDEO_REGION_DETECT_LEAVE = 11,//区域侦测离开
	ALARM_AI_VIDEO_REGION_DETECT_STAY = 12,//区域侦测逗留

	ALARM_AI_VIDEO_FALLINGOBJECT = 13,//高空抛物
	ALARM_AI_TEMP_UPPER = 14,//温度过高
	ALARM_AI_TEMP_LOWER =15,//温度过低
	ALARM_AI_HUMID_UPPER =16,//湿度过高
	ALARM_AI_HUMID_LOWER =17,//湿度过低
	ALARM_AI_VOC_THREASHHOLD_GOOD = 18,//空气优良门限300
	ALARM_AI_VOC_THREASHHOLD_TRACEPOLLUTION = 19,//微量污染门限1500
	ALARM_AI_VOC_THREASHHOLD_LIGHTPOLLUTION = 20,//轻度污染门限3000
	ALARM_AI_VOC_THREASHHOLD_MODERATEPOLLUTION = 21,//中度污染门限5000
	ALARM_AI_VOC_THREASHHOLD_HEAVYPOLLUTION = 22,//重度污染10000
	
	ALARM_AI_FALL = 23,	//跌倒
	ALARM_AI_GASTANK = 24,	//煤气罐
	ALARM_AI_MAX
}AjAiAlarmType;

typedef enum
{
    ALARM_PD_ID_HUMAN = 0,                  // 人形
    ALARM_PD_ID_BICYCLE = 1,                // 单车
    ALARM_PD_ID_CAR = 2,                    // 小汽车
    ALARM_PD_ID_MOTOR = 3,                  // 摩托车
    ALARM_PD_ID_BUS = 4,                    // 大巴
    ALARM_PD_ID_TRUCKS = 5,                 // 货车
    ALARM_PD_ID_HUMAN_CAR_FIXED = 100,      // 人车混合
}AnjPdAlarmId;

#define ALARM_MAX_PAYLOAD_LEN     (220)
#define ALARM_MAX_SNAPPATH_LEN    (96)

typedef struct
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

    int alarm_code;
    int alarm_flag;
    int alarm_level;
    int alarm_chn;
    char alarm_payload[ALARM_MAX_PAYLOAD_LEN];
    char snap_path[ALARM_MAX_SNAPPATH_LEN];
} alarm_event_data;


int alarm_loopback_info_set(int enable);

int alarm_is_night_check();

int alarm_arming_with_muti_timespan_check(ArmingStruct *pstArmingSetting, int bNight);

int alarm_arming_with_timespan_check(ArmingMode enable_flag, int bNight, const TimeSpanCfg *ptimeSpan);


int anj_alarm_event_handle(int chn, AjAlarmCode code, int flag, int level, int newalarm, 
                                const char *data, const char* snapfile);

void alarm_link_ptz_action_stop();

#ifdef __cplusplus
}
#endif

#endif
