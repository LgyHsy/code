#ifndef _ANJ_CONFIG_OEM_H_
#define _ANJ_CONFIG_OEM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define AJ_XML_SPECIFIC_FILE_NAME "specific.xml"
#define OEM_SECOND_CONFIG_PATH "/mnt/nand/cust/default_2_priority.xml"
#define OEM_SECOND_INIT_FLAG_PATH "/mnt/nand/cust/first_init_second_config_flag"
#define OEM_SECOND_SAVE_FLAG_PATH "/mnt/nand/cust/default2_savexmlconfig"
#define OEM_SECOND_RESTORE_FLAG_PATH "/mnt/nand/cust/ConfigRestore_flag"
#define OEM_SECOND_GENERIC_DEFAULT_XML "/mnt/nand/cust/config.default.xml"

#define AJ_XML_TEST_WEBSITE_FILE_NAME "test_website.xml"
#define AJ_XML_TEST_WEBSITE4_LEN        100

#define AJ_OEM_STR_LEN 32
typedef struct
{
    char szDeviceType[AJ_OEM_STR_LEN];
    char szVersion[AJ_OEM_STR_LEN];
    char szBuildtime[AJ_OEM_STR_LEN];

    char szOemSN[AJ_OEM_STR_LEN]; // OEM序列号
    char szOemHWVersion[AJ_OEM_STR_LEN];
    char szOemEthMac[AJ_OEM_STR_LEN];
    char szOemWifiMac[AJ_OEM_STR_LEN];
    char szOemMBL[AJ_OEM_STR_LEN];      // 主板序列号
    char szOemLanguage[AJ_OEM_STR_LEN]; // 语言
} AjOemStruct;

typedef struct
{
    // Configuration
    char device_name[64];  // 设备名称
    char gb_publisher[64]; // 国标名称
    char time_xy[8];       // 时间位置
    char title_xy[8];      // title位置
    char lan[16];          // 语言
    char ie_lan[16];       // ie语言
    char password[64];     // 密码
    char title[200];       // title
    // Conctrol
    int audio_out;                    // 音频输出
    int audio_in;                     // 音频输入
    int sen_o_li;                     // 开灯灵敏度
    int sen_c_li;                     // 关掉灵敏度
    int li_pw;                        // 补光灯亮度
    int led;                          // 补光图像选项
    int brightness;                   // 亮度
    int saturation;                   // 饱和度
    int sharpness;                    // 锐度
    int contrast;                     // 对比度
    char resolution_value_0[64];      // 主码流分辨率
    char resolution_value_1[64];      // 子码流分辨率
    char resolution_value_2[64];      // 第三码流分辨率
    char resolution_value_0_fake[64]; // 主码流假分辨率
    char resolution_value_1_fake[64]; // 子码流假分辨率
    char resolution_value_2_fake[64]; // 第三码流假分辨率
    int encoder_0;                    // 主码流编码类型
    int encoder_1;                    // 子码流编码类型
    int encoder_2;                    // 第三码流编码类型
    int privacy_ptz_direction_value;  //
    int tvsystem;                     // TV制式
    int kc_mode;                      // KC模式
    int ptz_speed;                    // 云台速度
    int low_li_pw;                    //
    int optimumdistance;              //
    int alarm_audio_switch;           // 报警声音开关
    // Capability
    int ptz_yuntai; // 云台
    int ptz_zoom;   // 放大
    int ptz_af;     // 聚焦+光圈
    int ptz_track;  // 跟踪
    int ptz_cruise; //
    int cover;      // 遮挡
    int call;       //
    int low_pw;     // 节能
    int led_type;   // 灯板类型
    int aov_workmode;
} SECOND_DEFAULTCONFIG_DATA;

typedef struct
{
	char name[AJ_OEM_STR_LEN];
	char location[AJ_OEM_STR_LEN];
	char city[AJ_OEM_STR_LEN];	
	char manufacturer[AJ_OEM_STR_LEN];	
	char model[AJ_OEM_STR_LEN];	
}OnvifOemStruct;

//Test web site,测试的网站最多三个
typedef struct
{
	char website1[AJ_OEM_STR_LEN];
	char website2[AJ_OEM_STR_LEN];
	char website3[AJ_OEM_STR_LEN];
	char website4[AJ_XML_TEST_WEBSITE4_LEN];
}TestWebSiteStruct;

typedef struct
{
	char httpsPrivateKeyPass[AJ_OEM_STR_LEN];
}HttpsPrivateStruct;

int anj_config_oem_second_load(SECOND_DEFAULTCONFIG_DATA *myconfig);
int anj_config_oem_second_gate_a(void);
int anj_config_oem_second_gate_b(void);

int anj_config_oem_factory_get(AjOemStruct *pInfo);

int anj_config_oem_get(AjOemStruct *pInfo);

int anj_config_oem_save(AjOemStruct *pInfo);

int anj_config_web_test_get(TestWebSiteStruct *pInfo);
int anj_config_oem_onvif_get(OnvifOemStruct *pInfo);
    
char *anj_config_oem_test_website_conver_msg_xml(TestWebSiteStruct *pCfg);

#ifdef __cplusplus
}
#endif

#endif
