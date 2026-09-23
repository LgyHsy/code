#ifndef _ANJ_CONFIG_MEDIA_H_
#define _ANJ_CONFIG_MEDIA_H_

#include "ixml.h"
#include "sdk_option.h"
#include "anj_mw_time.h"
#include "media_util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define VIDEO_PRIVATE_HEADER_MAGIC 0x1a2b3c4d
typedef struct
{
    unsigned long flag; // VIDEO_PRIVATE_HEADER_MAGIC
    unsigned long data;
    unsigned long frame_index;
    unsigned long keyframe_index;
} VIDEO_FRAME_HEADER;

#define TIME_FORMAT_MAX_LEN 32

typedef struct
{
    char format[TIME_FORMAT_MAX_LEN];
} TimeFormat;

typedef enum
{
    POSITION_TYPE_BY_FOUR_CORNER = 0, // 四角
    POSITION_TYPE_BY_SCALE = 1,       // 画面比例(0-100)
    POSITION_TYPE_DISABLE = 2,
} Positiontype;

typedef enum
{
    TYPE_TYPE_BY_TEXT = 0, // 文本
    TYPE_TYPE_BY_BMP = 1,  // BMP图片
} Titletype;

typedef enum
{
    AJ_OVERLAY_STYLE_BLACK_WHITE = 0,            // 黑字白底
    AJ_OVERLAY_STYLE_WHITE_BLACK = 1,            // 白字黑底
    AJ_OVERLAY_STYLE_TRANSPARENT_BLACKWHITE = 2, // 透明背景，黑字白框
    AJ_OVERLAY_STYLE_TRANSPARENT_WHITEBLACK = 3, // 透明背景，白字黑框
    AJ_OVERLAY_STYLE_TRANSPARENT_BLACK = 4,      // 透明背景，黑字
    AJ_OVERLAY_STYLE_TRANSPARENT_WHITE = 5,      // 透明背景，白字
    AJ_OVERLAY_STYLE_INVERSE_COLOR = 6,          // 反色
} AjOsdOverlayStyle;

typedef struct
{
    int posX; // 当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
    int posY; // 当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
    TimeFormat timeFormat;
    Positiontype posType;
} TimeOverlay;

#define TITLE_MAX_LEN 200

typedef enum
{
    TITLE_ADD_NOTHING = 0,
    TITLE_ADD_RESOLUTION,
    TITLE_ADD_BITRATE,
    TITLE_ADD_RESOLUTION_AND_BITRATE
} titleFormatEn;

typedef struct
{
    int posX;                       // 当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
    int posY;                       // 当posType=POSITION_TYPE_BY_FOUR_CORNER时，只能是0/1/2。否则为比例0-100%
    char title_utf8[TITLE_MAX_LEN]; // 当titleType=BMP时，这里存放BMP路径
    Positiontype posType;
    Titletype titleType;
} TitleOverlay;

typedef struct
{
    unsigned char overlay_enable;
    unsigned char overlay_transparency;
    unsigned short time_x;
    unsigned short time_y;
    unsigned short time_format_index;
    unsigned short title_x;
    unsigned short title_y;
    short style;
    short bDsplayWeek;
    char title_utf8[TITLE_MAX_LEN];
    short bOverlayFps;
    short fontsize;

    Positiontype timePostype;
    Positiontype titlePostype;
    Titletype titleType;
    short real_transparency;
    short time24or12;
} FEATURE_OVERLAY_CONFIG_PARAM;

typedef struct
{
    int enable;
    titleFormatEn transparency; // 叠加信息
    TimeOverlay timeOverlay;
    TitleOverlay titleOverlay;
    short style;       // AjOsdOverlayStyle
    short bDsplayWeek; // 0:不显示 1:在日期和时间的中间显示星期几 2:在末尾显示星期几
    short bOverlayFps;
    short fontsize;          // 0 标准  1 大字体 2 超大字体
    short real_transparency; // 透明度。0-100
    short time24or12;        // 0: 24 1: 12
} VideoOverlay;

typedef struct
{
    int enable;

    Positiontype posType;
    int pos_xscale; // 画面宽度比例位置0-100
    int pos_yscale; // 画面高度比例位置0-100

    int color_front; // 前景颜色
    int color_back;  // 背景颜色
    short style; // AjOsdOverlayStyle
    short transparency;             // 透明度。0-100
    unsigned char fontsize;         // 0 标准  1 大字体 2 超大字体
    unsigned char linegap;          // 0-8,0表示无间距，8表示间距1个字符高度
    char title_utf8[TITLE_MAX_LEN]; // 当titleType=BMP时，这里存放BMP路径
    Titletype titleType;
} UserOSD;

#define MAX_USER_OSD_NUM 5

typedef struct
{
    UserOSD data[MAX_USER_OSD_NUM];
} VideoUserOverlay;

#define RESOLUTION_NAME_MAX_LEN 32

typedef struct
{
    char name[RESOLUTION_NAME_MAX_LEN];
} Resolution;

typedef struct
{
    int enable;
    Resolution resolution;
    int frameRate;
    int format; // NV12/RGB888等等
} YuvEncodeCfg;

#define RTSP_MAX_VENC_CHN 4

#define VIDEO_FORMAT_MAX_LEN 32

typedef struct
{
    char name[VIDEO_FORMAT_MAX_LEN];
} VideoFormat;

typedef struct
{
    short shutter_mode_day;    // 0-1	//快门模式:自动/手动
    short shutter_mode_night;  // 0-1	//快门模式:自动/手动
    short shutter_speed_day;   // 10-10000	//快门速度
    short shutter_speed_night; // 10-10000 //快门速度
} VideoShutter;

typedef enum
{
    IRCUT_Mode_AGING_TEST = -1,      // 老化测试
    IRCUT_Mode_Active = 0,           // 主动模式/软光敏自动控制模式, ISP自动判断SENSOR增益，控制IRCUT和灯板
    IRCUT_Mode_DayNight = 1,         // 日夜模式，根据时间段来控制IRCUT和图像彩转灰
    IRCUT_Mode_Passive = 2,          // 被动模式/硬光敏外部控制模式，根据灯板的光敏电阻给的硬件信号，来控制IRCUT
    IRCUT_Mode_Manual = 3,           // 手动模式，不根据灯板和SENSOR增益，由调用者来手动切换
    IRCUT_Mode_ReversePassive = 4,   // 反向被动模式
    IRCUT_Mode_AUTO_BY_HARDWARE = 5, // 硬光敏自动控制模式，根据硬光敏的adc数据来切换日夜
    IRCUT_Mode_LIGHT_ALWAYS_ON = 6,  // 手动灯光常开
    IRCUT_Mode_LIGHT_ALWAYS_OFF = 7, // 手动灯光常关

    IRCUT_Mode_MAX
} IRCutMode;

typedef enum
{
    LED_PURE_INFRAED = 0,       // 纯红外
    LED_PURE_WHITE = 1,         // 纯白光
    LED_INFRAED_THEN_WHITE = 2, // 红外触发时白光
    LED_WHITE_THEN_INFRAED = 3, // 白光触发时红外
} LedMode;

typedef enum
{
    LED_IMAGE_NORMAL = 0,                               // 正常
    LED_IMAGE_FACE_EXPOSURE_PREVENTION = 1,             // 防人脸过曝
    LED_IMAGE_CHEPAI_MODE = 2,                          //: 照车牌模式 1
    LED_IMAGE_INTELLIGENT_FACE_EXPOSURE_PREVENTION = 3, // 智能防人脸过爆，告警触发时才启用防人脸过曝
    LED_IMAGE_CHEPAI_MODE_2 = 4,                        //: 照车牌模式 2
    LED_IMAGE_CHEPAI_MODE_3 = 5,                        //: 照车牌模式 3
    LED_IMAGE_CHEPAI_MODE_4 = 6,                        //: 照车牌模式 4
    LED_IMAGE_CHEPAI_MODE_5 = 7,                        //: 照车牌模式 5
    LED_IMAGE_CHEPAI_MODE_6 = 8,                        //: 照车牌模式 6
} LedImageMode;                                         // 补光图像模式

typedef enum
{
    IRCUT_OPENLED_ON_ILLUMINATION_0_01 = 0,  // 0.01
    IRCUT_OPENLED_ON_ILLUMINATION_0_05 = 1,  // 0.05
    IRCUT_OPENLED_ON_ILLUMINATION_0_08 = 2,  // 0.08
    IRCUT_OPENLED_ON_ILLUMINATION_0_10 = 3,  // 0.10
    IRCUT_OPENLED_ON_ILLUMINATION_0_20 = 4,  // 0.20
    IRCUT_OPENLED_ON_ILLUMINATION_0_30 = 5,  // 0.30
    IRCUT_OPENLED_ON_ILLUMINATION_0_40 = 6,  // 0.40
    IRCUT_OPENLED_ON_ILLUMINATION_0_50 = 7,  // 0.50
    IRCUT_OPENLED_ON_ILLUMINATION_0_60 = 8,  // 0.60
    IRCUT_OPENLED_ON_ILLUMINATION_0_70 = 9,  // 0.70
    IRCUT_OPENLED_ON_ILLUMINATION_0_80 = 10, // 0.80
    IRCUT_OPENLED_ON_ILLUMINATION_0_90 = 11, // 0.90
    IRCUT_OPENLED_ON_ILLUMINATION_1_00 = 12, // 1.0
    IRCUT_OPENLED_ON_ILLUMINATION_1_10 = 13, // 1.1
    IRCUT_OPENLED_ON_ILLUMINATION_1_20 = 14, // 1.2
    IRCUT_OPENLED_ON_ILLUMINATION_1_30 = 15, // 1.3
    IRCUT_OPENLED_ON_ILLUMINATION_1_40 = 16, // 1.4
    IRCUT_OPENLED_ON_ILLUMINATION_1_50 = 17, // 1.5
} IrcutOpenLedOnIllum;

typedef enum
{
    VIDEO_ISP_MODE_NORMAL = 0,          // 正常模式
    VIDEO_ISP_MODE_FORCE_FRAMERATE = 1, // 强制帧率模式
    VIDEO_ISP_MODE_SUPERSTAR = 2,       // 超星光模式
    VIDEO_ISP_MODE_LOW_POWER = 3,       // 低功耗模式
} VideoEncodeMode;

typedef enum
{
    VIDEO_WDR_MODE_OFF = 0,
    VIDEO_WDR_MODE_WDR = 1,         // 宽动态始终开启(325/328Q方案专指数字宽动态,早期方案自动根据SENSOR来确定数字/物理宽动态)
    VIDEO_WDR_MODE_WDR_BY_TIME = 2, // 按配置的时间段来宽动态
    VIDEO_WDR_MODE_HDR = 3,         // HDR(SENSOR物理宽动态，启动参数不一样，更改后需要重启摄像机)
} VideoWdrMode;

typedef enum
{
    LED_BRIGHTNESS_MODE_AUTO = 0,
    LED_BRIGHTNESS_MODE_MANUAL = 1,
    /* 扩展亮度模式，当前灯控仍按动态调光类模式处理。 */
    LED_BRIGHTNESS_MODE_EXT_2 = 2,
    LED_BRIGHTNESS_MODE_EXT_3 = 3,
} LedBrightnessMode;

typedef struct
{
    int enable;
    int autocrop;     // 自动裁剪
    int diameter_ppm; // 直径万分比
    int center_ppm_x; // X方向万分比
    int center_ppm_y; // Y方向万分比
} FishEyeCfg;

typedef struct
{
    int brightness;
    int contrast;
    int saturation;
    int sharpness;
    int daynight;

    unsigned char tvsystem;          // 0: NTSC (60HZ) 1: PAL (50HZ)
    unsigned char forct_antiflicker; // 强制抗闪，能力集"antiflicker"
    short reserved;
    unsigned short cropxpix; // X轴裁剪像素, 能力集"video_crop"
    unsigned short cropypix; // Y轴裁剪像素能力集"video_crop"

    int hflip;  // 水平翻转 0 1
    int vflip;  // 垂直翻转 0 1
    int rotate; // 走廊模式 0 1 , 能力集"rotate_enable"

    int whitebalance; // enable R G B 4个值组合
                        // enable=(whitebalance>>24)&0xff; R=(whitebalance>>16)&0xff; G=(whitebalance>>8)&0xff; B=(whitebalance)&0xff
    int backlight;    // 背光(逆光补偿0-255)
    int HLC;          // 强光抑制 //0-255
    int tnf;          // 2d降噪 //0-255
    int snf;          // 3D降噪 //0-255

    ////增益配置////
    int bManualGain; // 0: 自动增益 1: 手动增益, 能力集"gainsetting"
    int gainValue;   // 手动增益值, 能力集"gainsetting"

    ////////宽动态////////
    int wdr_mode;             // VideoWdrMode, 能力集"wdr_setting"
    DayTimeSpan wdr_worktime; // 能力集"wdr_setting"
    int wdr_value;            // 能力集"wdr_setting"

    ////////去雾////////
    int dfrog_flag;
    int dfrog_value;

    ////////电子快门////////
    VideoShutter shutterSetting; // 能力集"VideoShutter"

    // 图像ISP效果选项
    int isp_mode_color; // ISP彩色模式 0-3
    int isp_mode_night; // ISP夜间模式 0-3

    // 图像模式
    int videoEncodeMode; // VideoEncodeMode, 能力集"vencodemode_set"

    ////////IRCUT与补光相关////////
    IRCutMode ircut_mode;               // 能力集"ircut_setting"
    unsigned char ircut_sensitivity;    // 0 to 100 //未用到
    unsigned char ircut_openled_delay;  // 补光延时 //IrcutOpenLedOnIllum, 能力集"ircut_leddelay"
    unsigned char led_brightness_mode;  // 补光亮度控制:0自动 1手动， 能力集"ledtype_set"
    unsigned char led_brightness_value; // 补光亮度:10%-100%,  能力集"ledtype_set"
    unsigned char led_brightness_alarm; // 告警时补光亮度
    DayTimeSpan ircut_nighttime;        // 能力集"ircut_setting"
    int ircut_keepcolor;                // 20120419, 能力集"ircut_setting"
    LedMode led_mode;                   // 补光灯工作模式,能力集"ledtype_set"
    LedImageMode ispadvmode;            // 0: 正常 1: 防人脸过曝 2: 照车牌模式,  能力集"ledtype_set"
    ////////IRCUT与补光相关////////
    // 关灯灵敏度
    unsigned char light_off_sensitivity; // 能力集"ircut_leddelay"
    unsigned char face_exposure_sensitivity;

    // 鱼眼配置
    FishEyeCfg fishEyeCfg;

    int aov_mode;
    int aov_fps;
    unsigned short open_light;  /* 开灯档位，能力集 led_new */
    unsigned short close_light; /* 关灯档位，能力集 led_new */
} VideoCaptureCfg;

#define MAX_VIDEO_MASK_AREA 4
typedef struct
{
    int xPos;
    int yPos;
    int width;
    int height;
} MASK_AREA_ENTRY;

typedef struct
{
    MASK_AREA_ENTRY mainStreamMaskList[MAX_VIDEO_MASK_AREA];
    MASK_AREA_ENTRY subStreamMaskList[MAX_VIDEO_MASK_AREA];
} VideoMaskConfig;

#define MAX_VIDEO_ROI_AREA 4
typedef struct
{
    int xPos;
    int yPos;
    int width;
    int height;
} ROI_AREA_ENTRY;

typedef struct
{
    int enable;
    ROI_AREA_ENTRY roi[MAX_VIDEO_ROI_AREA];
} VideoROI;

#define VIDEO_ENCODE_FORAMT_MAX_LEN 32
typedef struct
{
    char name[VIDEO_ENCODE_FORAMT_MAX_LEN];
} VideoEncodeFormat;

#define BITRATE_CONTROL_MAX_LEN 32
typedef struct
{
    char name[BITRATE_CONTROL_MAX_LEN];
} BitRateControl;

typedef enum
{
    VIDEO_QUALITY_CUSTOM = 0, // 自定义
    VIDEO_QUALITY_WORSER = 1, // 更差
    VIDEO_QUALITY_WORSE = 2,  // 较差
    VIDEO_QUALITY_NORMAL = 3, // 正常
    VIDEO_QUALITY_GOOD = 4,   // 好
    VIDEO_QUALITY_BEST = 5,   // 更好
} VideoQualityEnum;

typedef struct
{
    int qp_enable; // 自定义QP, 为0时系统自动设置
    int qp_min;    // QP最小值,0-51，QP越小画面越精细，每帧的大小就越大；相反，QP越大画面越粗糙，占用的存储空间就越小
    int qp_max;    // QP最大值,0-51，同上，需要保证不小于qp_min
} QPConfig;

typedef struct
{
    int lbr_enable;
    int lbr_style;       // 低码率模式:	0: 保持帧率,自动码率	1: 视频质量优先,自动丢帧
    int lbr_bitratemode; // 码率控制:	0: 自动 1:手动
    int lbr_bitrate;     // 低码率目标值
    int lbr_motionlevel; // 运动级别:	0: 静止 1:运动幅度小 2:运动幅度大
    int lbr_noicelevel;  // 噪点级别:	0: 无 1:低 2:高
} LbrControl;

typedef struct
{
    int enable;
    int streamID;
    Resolution resolution;
    VideoEncodeFormat encodeFormat;
    BitRateControl bitRateControl;
    int initQuant;
    int bitRate;
    int frameRate;
    int display_frameRate; // 显示帧率，用于IPC返回数据呈现给用户界面，设置时需要填写真实帧率
    LbrControl lbrConfig;
    VideoQualityEnum bitRateQuality; // 码率质量，VIDEO_QUALITY_CUSTOM时bitRate有效，其他值时自动计算bitRate，避免用户填写bitRate值
    QPConfig qp;                     // VBR时自定义QP设置，需要有能力集FUNCTION_QP。VBR时此项才有效
} VideoEncodeCfg;

typedef struct
{
    int enable;  // 禁用/启用
    int quality; // 20-100
} JpegEncodeCfg;

typedef enum
{
    TWO_LENS_WORKMODE_JOINT_AND_CORRECTION = 0,  // 拼接并校正
    TWO_LENS_WORKMODE_LEFT_LEN = 1,              // 左镜头
    TWO_LENS_WORKMODE_RIGHT_LEN = 2,             // 右镜头
    TWO_LENS_WORKMODE_JOINT_NOT_CORRECTION = 3,  // 拼接不校正
    TWO_LENS_WORKMODE_TWOSTREAM_Independent = 4, // 双镜头分开出流
} TwoLensWorkMode;

typedef struct
{
    TwoLensWorkMode eTwoLensWorkMode;
    unsigned int nOptimumDistance; // 最佳距离
} TwoLensConfig;

typedef struct
{
    VideoEncodeCfg encodeCfg[MAX_VENC_CHN];
    // encode profile, 0: default, 1: baseprofile
    int encode_profile;
    int disable_private_data; // 0: enalbe, 1: disable
    int encode_mode;          // 0,4
    int noice_level;          // 0-10
    int ssvcEnable;
    TwoLensConfig twoLensCfg;
} VideoEncode;

typedef struct
{
    VideoCaptureCfg videoCapture;
    VideoEncode videoEncode;
    JpegEncodeCfg jpegCfg;
    VideoOverlay overlay;
    VideoMaskConfig videoMask;
    VideoROI roiCfg;
    VideoUserOverlay useroverlay;
    YuvEncodeCfg yuvCfg;
} VideoConfig;

typedef struct
{
    int channels;
    int bitspersample;
    int samplerate;
    short volume_capture;
    short volume_play;
    int amplify;     // 是否需要内部功放
    short ra_answer; // 反向音频是否需要按键接听
    short aec_enable;
    short mute_ptz_turn; // 云台转动时静音
    short reserved;
} AudioCapture;
#define AUDIO_ENCODE_TYPE_MAX_LEN 32

typedef struct
{
    char typeName[AUDIO_ENCODE_TYPE_MAX_LEN];
} AudioEncodeType;

typedef struct
{
    int enable;
    int sampleRate;
    AudioEncodeType audioEncodeType;
    int bitRate;
} AudioEncode;

typedef struct
{
    AudioCapture audioCapture;
    AudioEncode audioEncode;
} AudioConfig;

typedef struct
{
    VideoConfig videoConfig[ANJ_CAMERA_MAX_NUMS];
    AudioConfig audioConfig;
} MediaConfig;

int anj_config_media_default(MediaConfig *pMediaCfg);

int anj_config_video_capture_defalut(VideoCaptureCfg *pVideoCapture);

int anj_config_media_get(IXML_Node *pNode, MediaConfig *pMediaCfg);
int anj_config_video_capture_get(IXML_Node *pNode, VideoCaptureCfg *pVideoCaptureCfg);
int anj_config_video_encode_get(IXML_Node *pNode, VideoEncode *pVideoEncode);
int anj_config_overlay_get(IXML_Node *pNode, VideoOverlay *pVideoOverlay);
int anj_config_user_overlay_get(IXML_Node *pNode, VideoUserOverlay *pCfg);
int anj_config_video_mask_get(IXML_Node *pNode, VideoMaskConfig *pVideoMask);

char *anj_config_video_capture_conver_xml(VideoCaptureCfg *pCfg);
char *anj_config_video_encode_conver_xml(VideoEncode *pCfg);
char *anj_config_overlay_conver_xml(VideoOverlay *pCfg);
char *anj_config_user_overlay_conver_xml(VideoUserOverlay *pCfg);
char *anj_config_video_mask_conver_xml(VideoMaskConfig *pCfg);

int anj_config_media_save(MediaConfig *pMediaCfg);

int anj_config_media_set(MediaConfig *pstMediaConfig);

int anj_config_video_set(VideoConfig *pstVideoConfigArray);

int anj_config_video_capture_set(VideoCaptureCfg *pstVideoCapture, int cameraIndex);

int anj_config_overlay_set(VideoOverlay *pstVideoOverlay, int cameraIndex);

int anj_config_user_overlay_set(VideoUserOverlay *pstVideoUserOverlay, int cameraIndex);

int anj_config_video_mask_set(VideoMaskConfig *pstVideoMask, int cameraIndex);

int anj_config_video_roi_set(VideoROI *pstVideoRoi, int cameraIndex);

int anj_config_video_yuv_set(YuvEncodeCfg *pstVideoYuv, int cameraIndex);

int anj_config_video_encode_set(VideoEncode *pstVideoEncode, int cameraIndex);

int anj_config_jpeg_encode_set(JpegEncodeCfg *pstVideoJpeg, int cameraIndex);

int anj_config_audio_set(AudioConfig *pstAudioConfig);

int anj_config_audio_capture_set(AudioCapture *pstAudioCapture);

int anj_config_audio_encode_set(AudioEncode *pstAudioEncode);

int anj_config_media_load(MediaConfig *pMediaCfg);

char *anj_config_audio_encode_conver_xml(AudioEncode *pCfg);
char *anj_config_audio_capture_conver_xml(AudioCapture *pCfg);
char *anj_config_audio_conver_xml(AudioConfig *pAudioCfg);
char *anj_config_video_fisheye_conver_xml(FishEyeCfg *pCfg);
char *anj_config_video_capture_conver_xml(VideoCaptureCfg *pCfg);
char *anj_config_video_encode_conver_xml(VideoEncode *pCfg);
char *anj_config_jpeg_conver_xml(JpegEncodeCfg *pCfg);
char *anj_config_video_mask_conver_xml(VideoMaskConfig *pCfg);
char *anj_config_video_roi_conver_xml(VideoROI *pCfg);
char *anj_config_video_yuv_conver_xml(YuvEncodeCfg *pCfg);
char *anj_config_user_overlay_conver_xml(VideoUserOverlay *pCfg);
char *anj_config_overlay_conver_xml(VideoOverlay *pCfg);
char *anj_config_video_conver_xml(VideoConfig *pVideoCfg, int camera_index, int bMsg);
char *anj_config_media_conver_xml(MediaConfig *pMediaCfg, int camera_index, int bMsg);

int anj_config_audio_capture_get_by_xml(AudioCapture *pCaptureCfg, char *xmlBuf);
int anj_config_audio_encode_get_by_xml(AudioEncode *pEncodeCfg, char *xmlBuf);
int anj_config_audio_get_by_xml(AudioConfig *pAudioCfg, char *xmlBuf);
int anj_config_jpeg_encode_get_by_xml(JpegEncodeCfg *pJpegCfg, char *xmlBuf);
int anj_config_video_capture_get_by_xml(VideoCaptureCfg *pVideoCapture, char *xmlBuf, int MsgSrc);
int anj_config_video_encode_get_by_xml(VideoEncode *pVideoEncode, char *xmlBuf);
int anj_config_video_mask_get_by_xml(VideoMaskConfig *pVideoMask, char *xmlBuf);
int anj_config_overlay_get_by_xml(VideoOverlay *pVideoOverlay, char *xmlBuf);
int anj_config_video_roi_get_by_xml(VideoROI *pCfg, char *xmlBuf);
int anj_config_user_overlay_get_by_xml(VideoUserOverlay *pCfg, char *xmlBuf);
int anj_config_video_yuv_get_by_xml(YuvEncodeCfg *pCfg, char *xmlBuf);
int anj_config_video_get_by_xml(VideoConfig *pVideoCfgArray, char *xmlBuf);
int anj_config_media_get_by_xml(MediaConfig *pMediaCfg, char *xmlBuf);

char *anj_config_media_time_list_get(int index);

int anj_config_meida_flip_set(unsigned char vflip, unsigned char hflip, int cameraIndex);
int anj_config_image_flip_trans(int hflip, int vflip, int *newHflip, int *newVfilp);

int anj_config_audio_param_get(media_codec_type_e *audio_type, int *samplerate, int *bitspersample, int *channels);

#ifdef __cplusplus
}
#endif

#endif
