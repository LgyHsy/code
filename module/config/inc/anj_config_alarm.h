#ifndef _ANJ_CONFIG_ALARM_H_
#define _ANJ_CONFIG_ALARM_H_

#include "ixml.h"
#include "anj_mw_time.h"

#ifdef __cplusplus
extern "C"
{
#endif

// 部分报警联动取值从0/1扩充为0-3
typedef enum
{
    ARMING_DISABLE = 0, // 不布防
    ARMING_ALLDAY = 1,  // 全天布防
    ARMING_DAYTIME = 2, // 白天布防
    ARMING_NIGHT = 3,   // 夜晚布防
    ARMING_CUSTOM = 4,  // 自定义时间段
} ArmingMode;

#define DAY_TIMESPAN_MAX_NUM 4
// 布防配置:增加按时间段，只允许配置一天内4个时间段
typedef struct
{
    ArmingMode enable_flag;    // 0禁用 1全天 2白天 2晚上参见ArmingMode
    unsigned int timespan_num; // 时间段个数
    DayTimeSpan timeSpans[DAY_TIMESPAN_MAX_NUM];
} ArmingStruct;

#define MAX_PTZPOSITION_COUNT 20

typedef struct
{
    int positionIndex;
} PTZPosition;

typedef struct
{
    PTZPosition postion;
} PositionPreset;

typedef struct
{
    int interval;
    int duration;
    int positionCount;
    PTZPosition ptzPositions[MAX_PTZPOSITION_COUNT];
} PositionLoop;

typedef struct
{
    int interval;
    int walkCount;
    int positionCount;
    PTZPosition ptzPositions[MAX_PTZPOSITION_COUNT];
} PositionWalk;

#define PTZ_ACTION_TYPE_MAX_LEN 32
typedef struct
{
    char actionName[PTZ_ACTION_TYPE_MAX_LEN];
} PTZActionType;

typedef struct
{
    int enable;
    PTZActionType actionType;
    union Action
    {
        PositionPreset preset;
        PositionLoop loop;
        PositionWalk walk;
    } action;
} PTZAction;

/*
HIGH:对应常开，平时为0，触发时写1
LOW:对应常闭，平时为1，触发时写0
*/
#define TRIGGER_TYPE_NAME_MAX_LEN 32
typedef struct
{
    char name[TRIGGER_TYPE_NAME_MAX_LEN];
} TriggerType;

#define CHANNEL_TYPE_NAME_MAX_LEN 32
typedef struct
{
    char name[CHANNEL_TYPE_NAME_MAX_LEN];
} ChannelType;

typedef struct _OutputChannel
{
    int portIndex;
    ArmingStruct enable;
    int duration;
    ChannelType channelType;
    TriggerType triggerType;
} OutputChannel;

#define MAX_OUTPUT_CHANENL_COUNT 4

typedef struct
{
    int channelCnt;
    OutputChannel outputChannels[MAX_OUTPUT_CHANENL_COUNT];
} OutPutAlarm;

#define MAX_SMS_DESTNUM_LEN 64
#define MAX_SMS_FIX_CONTENT_LEN 128
typedef struct
{
    int enable;
    int min_interval; // 最小间隔时间
    int playresult;   // 语音播报发送结果
    char szSmsDstNum[MAX_SMS_DESTNUM_LEN];
    char szSmsFixContent[MAX_SMS_FIX_CONTENT_LEN];
} SMSAlarm;

typedef struct _OutputChannelAction
{
    int enable;
    int portIndex;
} OutputChannelAction;

typedef struct _OldOutputChannelAction
{
    int magicNum; // old cfg magic Num is 0xAABBCCDD
    int enable;
    int portIndex;
    int triggerType;
    int duration;
} OldOutputChannelAction;

typedef struct
{
    int channelCnt;
    union
    {
        OutputChannelAction outputChnlActions[MAX_OUTPUT_CHANENL_COUNT];
        OldOutputChannelAction oldOutputChnlAction;
    };
} AlarmOutputAction;

#define AUDIO_ACTION_LEN_FILENAME 128
typedef struct
{
    ArmingStruct enable;
    int times;           // 播放次数 <=0表示一直播。大于0表示次数
    int intervalsecnods; // 距离上次播放的最小间隔时间秒
    char filename[AUDIO_ACTION_LEN_FILENAME];
} AudioPlayAction;

typedef struct
{
    AlarmOutputAction outputAction;
    PTZAction ptzAction;
    AudioPlayAction audioAction;
    ArmingStruct light_twinkle_enable;      // 灯光闪烁
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct notify_sms;
} InputAction;

typedef struct
{
    int enable;
    int portIndex;
    ChannelType channelType;
    TriggerType triggerType;
    TimeSpanCfg timeSpan;
    InputAction alarmAction;
} AlarmChannel;

#define MAX_ALARMCHANNEL_COUNT 4

typedef struct
{
    int channelCnt;
    AlarmChannel alarmChannels[MAX_ALARMCHANNEL_COUNT];
} InputAlarm;

typedef struct
{
    AlarmOutputAction outputAction;
    PTZAction ptzAction;
    AudioPlayAction audioAction;
    ArmingStruct light_twinkle_enable;      // 灯光闪烁:
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_led_enable;          // 红蓝警灯
    ArmingStruct alarm_push;                // 报警推送
} MotionDetectAction;

typedef struct
{
    int enable_babycry; // 婴儿哭声
    int sensity_babycry;
    int enable_lsd; // 高分贝声音
    int sensity_lsd;
} AudioAlarm;

#define MD_MAX_GRID_ROW 18
#define MD_MAX_GRID_COL 22
#define MD_CONFIG_STRING_LEN (MD_MAX_GRID_ROW * MD_MAX_GRID_COL + 4)
#define MAX_MOTIONDETECT_CONFIG_STRING MD_CONFIG_STRING_LEN // 32

// 移动侦测组定义，左上、右下角block位置
typedef struct
{
    int left;
    int top;
    int right;
    int bottom;
} MdGroupConfig;

typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    TimeSpanCfg timeSpan;
    int blockCount;                                // low 16 bits for row , high 16 bits for column; //block_row=blockCount&0xffff;block_col=blockCount>>16;
    char blockCfg[MAX_MOTIONDETECT_CONFIG_STRING]; // ''\0' terminate
    int sensitivity;
    int alarmThreshold;
    int dayNightSwitch;
    int nightSensitivity;
    int nightAlarmThreshold;
    DayTimeSpan nightTime;
    MotionDetectAction alarmAction;
} MotionDetectAlarm;

typedef enum
{
    AI_TWINKLE_DISABLE = 0,
    AI_TWINKLE_REGION = 1, // 设防区域闪烁
    AI_TWINKLE_AROUND = 2, // 画面四周闪烁
} RectTwinkleEnum;

typedef struct
{
    unsigned char draw_rect_enable;    // 画区域框
    unsigned char draw_human_enable;   // 画人形框
    unsigned char track_human_enable;  // 人形跟踪
    unsigned char rect_twinkle_enable; // 检测到人形时区域框闪烁,参见RectTwinkleEnum
    unsigned char auto_zoom_enable;    // 变倍跟踪
    unsigned char gunball_track_mode;  // 枪球跟踪模式:0: 球跟踪 1: 枪球跟踪 2:枪球联动跟踪
    unsigned char track_time;
    unsigned char reserve3;
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
    ArmingStruct light_twinkle_enable;      // 灯光闪烁
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_led_enable;          // 红蓝警灯
    ArmingStruct alarm_push;                // 报警推送
} PdAction;

typedef struct
{
    unsigned char draw_rect_enable;    // 画区域框
    unsigned char draw_vehicle_enable; // 画车形框
    unsigned char rect_twinkle_enable; // 检测到车形时区域框闪烁
    unsigned char reserved;
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
} VehicleShapeAction;

typedef struct
{
    unsigned char draw_rect_enable;   // 画区域框
    unsigned char draw_target_enable; // 画检测目标
    unsigned char draw_osd_enable;    // 绘制车牌OSD
    unsigned char play_voice_enable;  // 语音播报

    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_push;                // 报警推送
} LprAction;

typedef struct
{
    int draw_rect_enable; // 画框
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
} FdAction;

typedef struct
{
    int xPos;
    int yPos;
    int width;
    int height;
} PD_AREA_ENTRY;

typedef struct _AJ_POINT_S
{
    int x;
    int y;
} AJ_POINT_S;

typedef struct
{
    double fX;
    double fY;
} AJ_POINT_F;

typedef struct _AJ_SIZE_S
{
    int u32Width;
    int u32Height;
} AJ_SIZE_S;

#define MAX_POLYGON_POINT_CNT 10
typedef struct _Polygon
{
    AJ_POINT_S points[MAX_POLYGON_POINT_CNT];
    int count;
} Polygon;

// 人形识别--20210611扩充成智能分析
typedef enum
{
    AI_TYPE_BIT_CAR = 0,             // 汽车
    AI_TYPE_BIT_MOTO = 1,            // 摩托车
    AI_TYPE_BIT_ELECTRICBICYCLE = 2, // 电单车
    AI_TYPE_BIT_BICYCLE = 3,         // 自行车
    AI_TYPE_BIT_HUMAN = 4,           // 人形
    AI_TYPE_BIT_FACE = 5,            // 人脸
    AI_TYPE_BIT_NONMOTO_VEHICLE = 6, // 非机动车
    AI_TYPE_BIT_FALLINGOBJECT = 7,   // 高空抛物
    AI_TYPE_BIT_GASTANK = 8,         // 煤气罐
    AI_TYPE_BIT_MAX,
} AjAiBits;

#define MAX_AJAIBITS_SIZE 32
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    unsigned int type;      // 32bit: 参见AjAiBits
    PD_AREA_ENTRY area;
    PdAction alarmAction;
    TimeSpanCfg timeSpan;          // 布防周计划，arming_flag配置为自定义时间段时生效
    int threshold;                 // 实际算法中没有用到threshold。配置界面上将sensitivity作为10位/threshold作为个位组合起来成为一个0-100的数来展示灵敏度
    int sensitivity;               // 历史原因这个值范围是0-10。配置界面上将sensitivity作为10位/threshold作为个位组合起来成为一个0-100的数来展示
    unsigned char minTargetRate;   // 检测目标最小画面比例,用于过滤小目标，0-100，0表示不过滤
    unsigned char nonMotionFilter; // 不动不检
    unsigned char allowMd;         // 布防时允许MD
    unsigned char reserved;        //
    Polygon polygonArea;
    unsigned char sensitivitys[MAX_AJAIBITS_SIZE]; // 每种目标检测的灵敏度，值范围0-100
} PdAlarm;

// 车牌识别
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    LprAction alarmAction;
    TimeSpanCfg timeSpan; // 布防周计划，arming_flag配置为自定义时间段时生效
    int sensitivity;      // 灵敏度:0-100

    Polygon polygonArea;
    int detectionmode;                         // 0:最优推图 1:间隔推图
    int actionInterval;                        // 两次上报时间间隔(相同车牌的情况下)
    char snapQuality[RESOLUTION_NAME_MAX_LEN]; // 抓图质量(分辨率)

} LprAlarm;

typedef struct
{
    unsigned char draw_rect_enable;   // 画区域框
    unsigned char draw_target_enable; // 画检测目标
    unsigned char reserve2;
    unsigned char reserve3;
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
    ArmingStruct light_twinkle_enable;      // 灯光闪烁
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_led_enable;          // 红蓝警灯
    ArmingStruct alarm_push;                // 报警推送
} FlameAndFlumesAction;

// 烟火报警检测
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    TimeSpanCfg timeSpan;   // 布防周计划，arming_flag配置为自定义时间段时生效

    Polygon polygonArea;
    short sensitivity;                         // 灵敏度:0-100
    short sensitivity_smog;                    // 灵敏度:0-100
    int actionInterval;                        // 两次上报时间间隔(相同车牌的情况下)
    char snapQuality[RESOLUTION_NAME_MAX_LEN]; // 抓图质量(分辨率)

    FlameAndFlumesAction alarmAction;
} FlameAndFlumesAlarm; //

//-------------------------//
typedef struct
{
    AlarmOutputAction outputAction;
} TempHumidityAction;

//-------------------------//

#define TEMP_ALARM_DESCRIBE "Temperature alarm"
#define HUMIDITY_ALARM_DESCRIBE "Humidity alarm"

typedef struct
{
    int temp_enable;
    int temp_upper_limit; // 温度报警上限
    int temp_lower_limit; // 温度报警下限
    int temp_range_lower; // 温度取值范围最小值
    int temp_range_upper; // 温度取值范围最大值

    int humidity_enable;
    int humidity_upper_limit; // 湿度报警上限
    int humidity_lower_limit; // 湿度报警下限
    int humidity_range_lower; // 湿度取值范围最小值
    int humidity_range_upper; // 湿度取值范围最大值

    int voc_enable;                        // 有毒气体检测启用
    int voc_threashhold_good;              // 空气优良门限（出厂默认300ppb）
    int voc_threashhold_TracePollution;    // 微量污染门限（出厂默认1500ppb）
    int voc_threashhold_LightPollution;    // 轻度污染门限（出厂默认3000ppb）
    int voc_threashhold_ModeratePollution; // 中度污染门限（出厂默认5000ppb）
    int voc_threashhold_HeavyPollution;    // 重度污染门限（出厂默认10000ppb）

    //	int  enable;
    TempHumidityAction alarmAction;
} TempHumidityAlarm;
//-------------------------//
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    PD_AREA_ENTRY area;
    FdAction alarmAction;
    TimeSpanCfg timeSpan;
    int threshold;
    int sensitivity;
} FaceDetectAlarm;

typedef struct
{
    AlarmOutputAction outputAction;
} VideoLostAction;

typedef struct
{
    int enable;
    TimeSpanCfg timeSpan;
    VideoLostAction alarmAction;
} VideoLostAlarm;

typedef struct
{
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
} VideoCoverAction;

typedef struct
{
    int enable;
    int sensitivity;
    int threadhold_second;      // 遮挡时间阈值(秒)：持续遮挡多长时间才认为是遮挡报警
    int backgroundUpdateSecond; // 背景帧更新时间（秒）：配置多长时间更新一次背景。用于设置M值，M默认为120，帧率为5时backgroundUpdateTime=24
    TimeSpanCfg timeSpan;
    VideoCoverAction alarmAction;
} VideoCoverAlarm;

typedef struct
{
    AlarmOutputAction outputAction;
} StorageFullAction;

typedef struct
{
    int enable;
    int threshold;
    StorageFullAction alarmAction;
} StorageFullAlarm;

typedef struct
{
    int enable;
    int sensitivity;
    unsigned int type; // 32bits,参见AT_TYPE_BIT_NONMOTO_VEHICLE定义。过滤检测:机动车、非机动车、行人
    int x0Pos;
    int y0Pos;
    int x1Pos;
    int y1Pos;
    int direction; // 0: A<->B 1: A->B 2: A<-B (SIDE A MEANS LEFT OF THE LINE)
} VideoLineStruct;

typedef struct
{
    unsigned char draw_rect_enable;   // 画检测配置线
    unsigned char draw_target_enable; // 画检测目标
    unsigned char reserve1;
    unsigned char reserve2;

    ArmingStruct light_twinkle_enable;      // 灯光闪烁
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_led_enable;          // 红蓝警灯
    ArmingStruct alarm_push;                // 报警推送
    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
} VideoGateAction;

#define MAX_VIDEO_VG_LINE 4
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    VideoLineStruct data[MAX_VIDEO_VG_LINE];
    VideoGateAction alarmAction;
    TimeSpanCfg timeSpan;
} VideoGateAlarm;

// 区域检测类型
typedef enum
{
    AI_RETION_STAY,  // 区域逗留
    AI_RETION_ENTER, // 进入区域
    AI_RETION_LEAVE, // 离开区域
} RegionAiMode;

typedef struct
{
    int enable;
    int sensitivity;
    unsigned int type_filter; // 检测目标类型，32bits,参见AT_TYPE_BIT_NONMOTO_VEHICLE定义。过滤检测:机动车、非机动车、行人
    RegionAiMode mode;        // 检测目标模式
    int stayseconds;          // 逗留时间(秒)，仅对区域逗留有效
} RegionAiUnitStruct;

typedef struct
{
    unsigned char draw_rect_enable;   // 画检测配置线
    unsigned char draw_target_enable; // 画检测目标
    unsigned char reserve1;
    unsigned char reserve2;

    ArmingStruct light_twinkle_enable;      // 灯光闪烁
    ArmingStruct notify_alarmserver_enable; // 上报告警中心
    ArmingStruct alarm_led_enable;          // 红蓝警灯
    ArmingStruct alarm_push;                // 报警推送

    AlarmOutputAction outputAction;
    AudioPlayAction audioAction;
} RegionAiAction;

#define MAX_VIDEO_REGION_AI_NUM 3
typedef struct
{
    int enable;             // 算法启用开关
    ArmingMode arming_flag; // 布防总开关(不影响检测，只影响联动)
    Polygon polygonArea;
    RegionAiUnitStruct data[MAX_VIDEO_REGION_AI_NUM];
    RegionAiAction alarmAction;
    TimeSpanCfg timeSpan;
} VideoRegionAiAlarm;

typedef struct
{
    PdAlarm pdAlarm[ANJ_CAMERA_MAX_NUMS];                  // 智能分析:人形、车型、摩托、电单车、自行车
    VideoGateAlarm vgAlarm[ANJ_CAMERA_MAX_NUMS];           // 越界检测
    VideoRegionAiAlarm regionAiAlarm[ANJ_CAMERA_MAX_NUMS]; // 区域检测:区域入侵、进入区域、离开区域
    FaceDetectAlarm fdAlarm[ANJ_CAMERA_MAX_NUMS];          // 人脸检测
    LprAlarm lprAlarm[ANJ_CAMERA_MAX_NUMS];                // 车牌识别
    FlameAndFlumesAlarm fireAlarm;                         // 烟火检测
    AudioAlarm audioAlarm;                                 // 异常声音检测(婴儿啼哭、高分贝)
} AlarmAIConfig;

typedef struct
{
    InputAlarm inputAlarm;                                    // IO输入
    MotionDetectAlarm motionDetectAlarm[ANJ_CAMERA_MAX_NUMS]; // 移动侦测
    VideoLostAlarm videoLostAlarm[ANJ_CAMERA_MAX_NUMS];       // 视频丢失
    VideoCoverAlarm videoCoverAlarm[ANJ_CAMERA_MAX_NUMS];     // 视频遮挡
    StorageFullAlarm storageFullAlarm;                        // 存储空间
    OutPutAlarm outputAlarm;                                  // IO输出配置
    TempHumidityAlarm temphumidityAlarm;                      // 温度报警
    SMSAlarm smsAlarm;                                        // 短消息通知
} AlarmNormalConfig;

typedef struct
{
    AlarmAIConfig aiAlarm;
    AlarmNormalConfig normalAlarm;
} AlarmConfig;

int anj_config_alarm_get(IXML_Node *pNode, AlarmConfig *pAlarmCfg, int camera_index, int bMsg);
int anj_config_alarm_motion_get(IXML_Node *pNode, MotionDetectAlarm *pAlmArray, int bMsg);
int anj_config_alarm_video_gate_get(IXML_Node *pNode, VideoGateAlarm *pAlmArray, int bMsg);
int anj_config_alarm_pd_get(IXML_Node *pNode, PdAlarm *pAlmArray, int bMsg);

int anj_config_alarm_audio_get_by_xml(AudioAlarm *pAlm, char *xmlBuf);
int anj_config_alarm_fd_get_by_xml(FaceDetectAlarm *pAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_fire_get_by_xml(FlameAndFlumesAlarm *pAlm, char *xmlBuf);
int anj_config_alarm_input_get_by_xml(InputAlarm *pInputAlm, char *xmlBuf);
int anj_config_alarm_input_channel_get_by_xml(AlarmChannel *pAlarmChannel, char *xmlBuf);
int anj_config_alarm_lpr_get_by_xml(LprAlarm *pAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_motion_get_by_xml(MotionDetectAlarm *pMDAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_output_get_by_xml(OutPutAlarm *pOutputAlm, char *xmlBuf);
int anj_config_alarm_output_channel_get_by_xml(OutputChannel *pAlarmChannel, char *xmlBuf);
int anj_config_alarm_pd_get_by_xml(PdAlarm *pAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_region_get_by_xml(VideoRegionAiAlarm *pAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_sms_get_by_xml(SMSAlarm *pAlm, char *xmlBuf);
int anj_config_alarm_storage_full_get_by_xml(StorageFullAlarm *pSFAlm, char *xmlBuf);
int anj_config_alarm_temp_humidity_get_by_xml(TempHumidityAlarm *pAlm, char *xmlBuf);
int anj_config_alarm_video_cover_get_by_xml(VideoCoverAlarm *pVideoCover, char *xmlBuf, int camera_index);
int anj_config_alarm_video_gate_get_by_xml(VideoGateAlarm *pAlm, char *xmlBuf, int camera_index);
int anj_config_alarm_video_lost_get_by_xml(VideoLostAlarm *pVideoLost, char *xmlBuf, int camera_index);
int anj_config_alarm_get_by_xml(AlarmConfig *pAlarmCfg, char *xmlBuf, int camera_index);

int anj_config_alarm_save(AlarmConfig *pAlarmCfg);

int anj_config_alarm_set(AlarmConfig *pstAlarmConfig);
int anj_config_alarm_fd_set(FaceDetectAlarm *pstFdAlarmArray);
int anj_config_alarm_pd_set(PdAlarm *pstPdAlarmArray);
int anj_config_alarm_video_gate_set(VideoGateAlarm *pstVgAlarmArray);
int anj_config_alarm_region_set(VideoRegionAiAlarm *pstVrAlarmArray);
int anj_config_alarm_lpr_set(LprAlarm *pstLprAlarmArray);
int anj_config_alarm_fire_set(FlameAndFlumesAlarm *pstFireAlarmArray);
int anj_config_alarm_audio_set(AudioAlarm *pstAudioAlarmCfg);
int anj_config_alarm_input_channel_set(AlarmChannel *pstAlarmChannel, int channel);
int anj_config_alarm_input_set(InputAlarm *pstInputAlarmCfg);
int anj_config_alarm_motion_set(MotionDetectAlarm *pstMdAlarmArray);
int anj_config_alarm_video_lost_set(VideoLostAlarm *pstVideoLostAlarmArray);
int anj_config_alarm_video_cover_set(VideoCoverAlarm *pstVideoCoverAlarmArray);
int anj_config_alarm_storage_full_set(StorageFullAlarm *pstStoreFullAlarmCfg);
int anj_config_alarm_output_channel_set(OutputChannel *pstOutputChannel, int channel);
int anj_config_alarm_output_set(OutPutAlarm *pstOutputAlarmCfg);
int anj_config_alarm_temphumidity_set(TempHumidityAlarm *pstTempAlarmCfg);
int anj_config_alarm_sms_set(SMSAlarm *pstSmsAlarmCfg);

int translate_old_output_action(AlarmOutputAction *pIoOutput, OutPutAlarm *pOutputAlarm);

int anj_config_alarm_read_lpr_config(AlarmOutputAction *p_ioOutput, AudioPlayAction *p_audioOutput);

char *anj_config_alarm_arming_daytimesapn_conver_xml(const char *TimeSpanName, ArmingStruct *pData);
char *anj_config_alarm_audio_action_conver_xml(AudioPlayAction *pAction);
char *anj_config_alarm_ptz_action_preset_conver_xml(PositionPreset *pPreset);
char *anj_config_alarm_ptz_action_loop_conver_xml(PositionLoop *pLoop);
char *anj_config_alarm_ptz_action_walk_conver_xml(PositionWalk *pWalk);
char *anj_config_alarm_output_action_conver_xml(AlarmOutputAction *pAlmOutputAction);
char *anj_config_alarm_ptz_action_conver_xml(PTZAction *pPtzAction);
char *anj_config_alarm_storage_full_conver_xml(StorageFullAlarm *pStorageFullAlm);
char *anj_config_alarm_audio_conver_xml(AudioAlarm *pAlm);
char *anj_config_alarm_region_ai_conver_xml(VideoRegionAiAlarm *pAlm, int camera_index, int bMsg);
char *anj_config_alarm_sms_conver_xml(SMSAlarm *pAlm);
char *anj_config_alarm_lpr_conver_xml(LprAlarm *pAlm);
char *anj_config_alarm_fire_conver_xml(FlameAndFlumesAlarm *pAlm);
char *anj_config_alarm_temp_humidity_conver_xml(TempHumidityAlarm *pAlm);
char *anj_config_alarm_fd_conver_xml(FaceDetectAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_video_cover_conver_xml(VideoCoverAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_video_lost_conver_xml(VideoLostAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_motion_conver_xml(MotionDetectAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_pd_conver_xml(PdAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_video_gate_conver_xml(VideoGateAlarm *pAlmArray, int camera_index, int bMsg);
char *anj_config_alarm_input_conver_xml(InputAlarm *pInputAlm);
char *anj_config_alarm_output_conver_xml(OutPutAlarm *pOutputAlm);
char *anj_config_alarm_conver_xml(AlarmConfig *pAlarmCfg);

#ifdef __cplusplus
}
#endif

#endif
