#ifndef _PROJECT_OPTION_H_
#define _PROJECT_OPTION_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define ANJ_PROJECT_NAME "D72W"
#define ANJ_CUSTOMER_TYPE (CUSTOMER_WTD)
#define ANJ_PROJECT_SENSOR (SENSOR_TYPE_OS03A10)
#define ANJ_PROJECT_TYPE (PROJECT_TYPE_NORMAL)
#define ANJ_DEVICE_TYPE ""
#define ANJ_CRYPT_NAME "NULL"
#define ANJ_CAMERA_MAX_NUMS (1)
#define MAX_SCL_PORT (2)
#define MAX_VENC_CHN (2)
#define MAX_VIDEO_NUM (ANJ_CAMERA_MAX_NUMS * MAX_VENC_CHN)
// #define PRODCUT_CODE "cmcc"
#define PRODCUT_CODE "public"

//***********************************设备网络相关配置***********************************//
#define IPC_NETWORK_TYPE (NET_DEV_TYPE_WIRE_WIFI)
#define IPC_USB_GPIO_DEFAULT_VALUE (1)
//***********************************设备4G相关配置***********************************//
#define IPC_4G_SIMCARD_NUM (0)
#define IPC_4G_USE_USB0 (0)
#define IPC_4G_CONFIG_SET_ENABLE (0)
#define IPC_4G_UPDATE_FIRMWARE (0)
#define IPC_4G_EXTRA_SIM_ENABLE (0)
//***********************************设备WIFI相关配置***********************************//
#define IPC_WIFI_SUPPORT_AP (1)
#define IPC_PROMPT_WIRE_USE_DETAIL (0) /* 1=有线配网用明细提示 */

//***********************************设备灯光相关配置***********************************//
#define LIGHT_PWM_MAX_VALUE (50000)
#define NIGHT_REDUCE_FPS (5)
#define SUPPORT_WEAK_LIGHT (1)

/* 软光敏阈值（BV）：{delay_idx, open_bv, reserved}；回差与类型逻辑在 anj_ispctl.c */
#define ISP_LIGHT_DELAY_ENTRY   \
    {0,  3500, 20},     \
    {1,  3200, 25},     \
    {2,  3000, 28},     \
    {3,  2800, 30},     \
    {4,  2600, 35},     \
    {5,  2400, 40},     \
    {6,  2200, 100},    \
    {7,  2000, 100},    \
    {8,  1800, 100},    \
    {9,  1600, 100},    \
    {10, 1400, 100},    \
    {11, 1200, 100},    \
    {12, 1000, 100},    \
    {13, 900, 100},     \
    {14, 800, 100},     \
    {15, 700, 100},     \
    {16, 600, 100},     \
    {17, 500, 100},     \
    {0, 0, 0},

//***********************************媒体能力集配置***********************************//
#define RESOLUTION_LIST                                               \
	{"2560X1440", "H265+", 0, 3500, 500, 6000, 25, 5, 25, 0, 0, 25},  \
	{"2304X1296", "H265+", 0, 3000, 500, 6000, 25, 5, 30, 0, 0, 30},  \
	{"1080P",     "H265+", 0, 2500, 200, 8000, 25, 5, 30, 1, 0, 30},  \
	{"720P",      "H265+", 0, 1500, 100, 5000, 25, 5, 30, 1, 0, 30},  \
	{"640X360",   "H265+", 1, 500,  50,  1000, 25, 5, 30, 0, 0, 30},  \
	{"480X360",   "H265+", 1, 500,  50,  1000, 25, 5, 30, 0, 0, 30},  \
	{"CIF",       "H265+", 1, 150,  50,  2000, 25, 5, 30, 0, 0, 30},  \
	{"2560X1440", "H265",  0, 3500, 500, 6000, 25, 5, 25, 0, 1, 25},  \
	{"2304X1296", "H265",  0, 3000, 500, 6000, 25, 5, 30, 0, 0, 30},  \
	{"1080P",     "H265",  0, 2500, 200, 8000, 25, 5, 30, 1, 0, 30},  \
	{"720P",      "H265",  0, 1500, 100, 5000, 25, 5, 30, 1, 0, 30},  \
	{"640X360",   "H265",  1, 500,  50,  1000, 25, 5, 30, 0, 1, 30},  \
	{"480X360",   "H265",  1, 500,  50,  1000, 25, 5, 30, 0, 0, 30},  \
	{"CIF",       "H265",  1, 150,  50,  2000, 25, 5, 30, 0, 0, 30},  \
	{"2560X1440", "H264",  0, 4000, 500, 8000, 25, 5, 25, 0, 0, 25},  \
	{"2304X1296", "H264",  0, 3500, 500, 8000, 25, 5, 30, 0, 0, 30},  \
	{"1080P",     "H264",  0, 3000, 500, 9000, 25, 5, 30, 1, 0, 30},  \
	{"720P",      "H264",  0, 2500, 500, 6000, 25, 5, 30, 1, 0, 30},  \
	{"640X360",   "H264",  1, 500,  50,  2000, 25, 5, 30, 0, 0, 30},  \
	{"480X360",   "H264",  1, 500,  50,  2000, 25, 5, 30, 0, 0, 30},  \
	{"CIF",       "H264",  1, 150,  50,  2000, 25, 5, 30, 0, 0, 30},  \
	{"", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 30}

#define AUDIO_CODEC_LIST             \
    {"G.711U", 1, 16, 8, 64, 1},     \
    {"G.711U", 1, 16, 16, 64, 0},    \
    {"G.711A", 1, 16, 8, 64, 0},     \
    {"G.711A", 1, 16, 16, 64, 0},    \
    {"AAC", 2, 16, 16, 16, 0},       \
    {"", 0, 0, 0, 0, 0}

#define YUV_LIST                  \
    {"1080P",    0, 5, 1, 10, 0}, \
    {"720P",     0, 5, 1, 10, 0}, \
    {"764X488",  0, 5, 1, 10, 0}, \
    {"640X480",  0, 5, 1, 10, 1}, \
    {"640X352",  0, 5, 1, 10, 0}, \
    {"608X352",  0, 5, 1, 10, 0}, \
    {"416X224",  0, 5, 1, 10, 0}, \
    {"384X288",  0, 5, 1, 10, 0}, \
    {"352X288",  0, 5, 1, 10, 0}, \
    {"",         0, 0, 0, 0, 0}

// 265+, avbr vbr cbr
#define VIDEO_RC_LIST                \
    {{36, 48}, {32, 48}, {20, 48}},  \
    {{30, 48}, {32, 48}, {20, 48}},  \
    {{30, 48}, {30, 48}, {20, 48}}

#define CLOUD_AUTH_CODE_NEED (0)
#define ANJ_CAMERA_MMA_SIZE "0x1560000"
#define ANJ_CAMERA_VIDEO_MAX_SIZE (250 * 1024) // VENC单帧最大长度
#define ANJ_CAMERA_VIDEO_SUB_MAX_SIZE (256 * 1024)
#define ANJ_CAMERA_AUDIO_MAX_SIZE (50 * 1024)


#define MAX_AUDIO_CAPTURE_VOLUME (100)
#define MAX_AUDIO_PLAY_VOLUME (100)

#define RESOLUTION_5MP_WIDTH (2880)
#define RESOLUTION_5MP_HEIGHT (1620)
#define RESOLUTION_MAX_WIDTH (2560)
#define RESOLUTION_MAX_HEIGHT (1440)

#define BUFSZIE_VENC0_COEF 15 / 37
#define BUFSZIE_VENC1_COEF 1

#define ISP_DAY_BIN_PATH "/config/iqfile/gc4023_day.bin"
#define ISP_NIGHT_BIN_PATH "/config/iqfile/gc4023_day.bin"
#define ISP_WDR_BIN_PATH ""
#define ISP_AI_BIN_PATH "/config/iqfile/sp4329_ai_day.bin"
#define ISP_AI_IR_BIN_PATH ""
#define ISP_DEFAUT_AE_TARGET (360)

#define AUDIO_PERIOD_SIZE (640)

#define MAX_OSD_CANVAS_NUM (1)
#define OSD_FULL_IMAGE (1)
#define OSD_SHOW_BAT_VOLT   0

#define AUDIO_BF_ENABLE         (0)
#define AUDIO_ANR_ENABLE        (1)
#define AUDIO_AGC_ENABLE        (0)
#define AUDIO_EQ_ENABLE         (0)

//***********************************硬件相关配置***********************************//
#define MOTO_PWM_ID0 4
#define MOTO_PWM_ID1 5
#define MOTO_PWM_ID2 6
#define MOTO_PWM_ID3 7
#define MOTO_PWM_GROUP_ID 1
 
#define ANJ_GPIO_PORT_SDCARD_POWER              45
#define ANJ_GPIO_PORT_GYRO_POWER                0
#define ANJ_GPIO_PORT_RESET                     44
#define ANJ_GPIO_PORT_SWITCH_AUDIOOUT           23
#define ANJ_GPIO_PORT_MOTOT_SEL                 43
#define ANJ_GPIO_PORT_SWITCH_LED                61
#define ANJ_GPIO_PORT_ALARM_LED                 75
#define ANJ_GPIO_PORT_SWITCH_SIMCARD            0
#define ANJ_GPIO_PORT_USB_POWER                 80
#define ANJ_GPIO_PORT_AOV_4G_RESUME             0
#define ANJ_GPIO_PORT_AOV_4G_STAT               0
#define ANJ_GPIO_PORT_AOV_CHARGE                0
#define ANJ_GPIO_PORT_ALARMIN_CHN1              0   
#define ANJ_GPIO_PORT_ALARMIN_CHN2              0 
#define ANJ_GPIO_PORT_ALARMIN_CHN3              0 
#define ANJ_GPIO_PORT_ALARMIN_CHN4              0 
#define ANJ_GPIO_PORT_ALARMOUT_CHN1             0 
#define ANJ_GPIO_PORT_ALARMOUT_CHN2             0
#define ANJ_GPIO_PORT_ALARMOUT_CHN3             0
#define ANJ_GPIO_PORT_ALARMOUT_CHN4             0
#define ANJ_ALARMIN_CLOSE_LEVEL                 0   // 物理低电平=闭合 
#define ANJ_GPIO_PORT_PHOTO_SENSOR              0   // 硬光敏输入，GPIO待定
#define ANJ_PHOTO_SENSOR_BRIGHT_LEVEL           (1) // 物理高电平=亮

#define ANJ_IPC_LIGHT_TYPE          (LIGHTBOARD_TYPE_WHITE)
#define ANJ_PWM_LIGHT_PORT_R        0
#define ANJ_PWM_LIGHT_PORT_W        0
#define ANJ_PWM_LIGHT_PORT2         0

#define ANJ_EXPAND_DEV_GPIO         (0)         // /dev/expand_dev驱动扩展gpio
#define ANJ_EXPAND_I2C_GPIO         (0)         // i2c扩展gpio

#define ANJ_I2C_GPIO_PORT_SDA       (0)
#define ANJ_I2C_GPIO_PORT_SCL       (0)

#define PTZ_H_MAX_STEP              (800) /*步进电机水平最大步数 5.625/25.4359 * 404 * 8 = 714.73783...*/
#define PTZ_V_MAX_STEP              (200) /*步进电机垂直最大步数 5.625/64 * 158 * 8 = 111.09375 */
#define SUPPORT_ADVANCE_PTZ         (1) 

//***********************************算法和告警支持***********************************//
#define ALARM_SUPPORT_SMART_AI_FACE_REC     (0)
#define ALARM_SUPPORT_SMART_AI_FIRE         (0)
#define ALARM_SUPPORT_SMART_AI_LPR          (0)

#define ALARM_SUPPORT_VIDEO_GATE            (1)
#define ALARM_SUPPORT_MOTION                (1)
#define ALARM_SUPPORT_REGION_AI             (0)
#define ALARM_SUPPORT_VIDEO_COVERD          (0)
#define ALARM_SUPPORT_IO_IN                 (0)
#define ALARM_SUPPORT_IO_OUT                (0)

#define SMART_PD_WIDTH (1920)
#define SMART_PD_HEIGHT (1080)
#define SMART_PD_IPU_PATH "/config/dla/ipu_lfw.bin"
#define SMART_PD_MODEL_PATH "/config/dla/model/share_spdy36.img"
#define DEFAULT_SMART_WIDTH (640)
#define DEFAULT_SMART_HEIGHT (352)
#define DEFAULT_SMART_FPS (25)
#define DEFAULT_SMART_DEPTH (2)

#define ANJ_LED_CFG_NEW (0)  /* 1=能力集 led_new，开关灯高中低三档 */

#ifdef __cplusplus
}
#endif
#endif
