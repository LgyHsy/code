#ifndef _____FUNCTION_LIST_H____
#define _____FUNCTION_LIST_H____

// for system control config
#define FUNCTION_SUPPORT_STORAGE "storage_support"

#define FUNCTION_WIRELESS_STATION "wireless_station"
#define FUNCTION_WIFI_AP "wifi_ap"
#define FUNCTION_WIFI_AP_STATION_SAMETIME "ap_station"
#define FUNCTION_MOBILE_NET "mobile_net"        // 移动网络，是否支持4G模块
#define FUNCTION_SMS "mobile_sms"               // 支持短消息
#define FUNCTION_CHECK_INTERNET "domains_check" // 支持域名检查消息
#define FUNCTION_DHCPIP_FIX "dhcp_fixtime"
#define FUNCTION_ENCRYPTIONCONFIG "encryptionconfig"

#define FUNCTION_NETWORK_STORAGE "network_storage"
#define FUNCTION_FTPEMAIL_STORAGE "ftpemail_aj"
#define FUNCTION_EMAIL_SSL "ssl_email"
#define FUNCTION_SCHEDULE_RECORD "schedule_record"
#define FUNCTION_PICTURE_CAPTURE "picture_capture"
#define FUNCTION_PTZ_CONTROL "ptz_control"
#define FUNCTION_GPIO_INPUT "gpio_input"
#define FUNCTION_GPIO_OUTPUT "gpio_output"
#define FUNCTION_PTZ_ACTION "ptz_action"
#define FUNCTION_AUDIOPLAY_ACTION "audio_action"
#define FUNCTION_AUDIOPLAY_ACTION_DAYNIGHT "audio_action_daynight" // 音频报警联动日夜分开配置
#define FUNCTION_LIGHT_ACTION "light_action"                       // 报警输出灯光闪烁
#define FUNCTION_ALARM_SERVER "alarm_server"                       // 上报报警服务器
#define FUNCTION_ALARM_LED "alarm_led"                             // 红蓝警灯
#define FUNCTION_LEDPANEL_IR "ir_led_panel"                        // 纯红外灯板
#define FUNCTION_LEDPANEL_WHITE "white_led_panel"                  // 纯白光灯板
#define FUNCTION_LEDPANEL_DOUBLE "double_led_panel"                // 智能双光灯板
#define FUNCTION_MANUAL_PTZ_SPEED "manual_set_ptz_speed"           // PTZ速度可控
#define FUNCTION_CAT_MANUAL_PTZ_SPEED "cat_manual_set_ptz_speed"   // 去掉PTZ速度可控

#define FUNCTION_PILOT_POWER_LIGHT "pilot_power_light" // Y10指示灯_电源灯的控制开关

#define FUNCTION_CLOUD_AUTHCODE "cloud_authcode" // 云ID授权码
#define FUNCTION_PT_3D "ptz3dlocate"          // 3d定位服务

#define FUNCTION_RA_ANSWER "ra_answer"
#define FUNCTION_AEC "aec_enable"
#define FUNCTION_MUTE_PTZ_TURN "mute_ptz_turn"
#define FUNCTION_AUDIOCAP_SAMPRATE "audiocap_samplerate"
#define FUNCTION_RA_PCM "ra_pcm"
#define FUNCTION_RA_MP3STREAM "ra_mp3"
#define FUNCTION_UPNP "enable_upnp"

#define FUNCTION_THREE_VIDEO "three_video"           // 是否支持VIDEO第三码流
#define FUNCTION_YUV_VIDEO "yuv_video"               // 是否支持YUV VIDEO 跨进程读取
#define FUNCTION_YUV_CFG "yuv_sizecfg"               // 是否支持YUV 尺寸配置
#define FUNCTION_ONLY_AUDIO "only_audio"             // 是否仅运行音频编码
#define FUNCTION_VIDEO_FORBIT "forbid_video"         // 是否可进制视频流
#define FUNCTION_VIDEO_ROI "video_roi"               // 是否可设置视频兴趣区域
#define FUNCTION_LED_TYPE "ledtype_set"              // 是否可设置白光/红外光
#define FUNCTION_VIDEO_ENCODE_MODE "vencodemode_set" // 是否可设置视频编码模式(图像模式)
#define FUNCTION_USEROSD "userosd_set"               // 是否可设置自定义OSD
#define FUNCTION_TITLE_BMP "bmplogo_set"             // 是否可设置BMP LOGO图片
#define FUNCTION_TITLE_COLOR "titlecolor_set"        // 是否可设置title颜色图片
#define FUNCTION_TITLE_TRANSPARENT "alpha_set"       // 是否可设置透明度

#define FUNCTION_FRONT_REPLAY "front_replay"
#define FUNCTION_REPLAY_BYTIME "replay_bytime"
#define FUNCTION_MEDIA_CAPABILITY "media_capabiltiy"
#define FUNCTION_IRCUT_SETTING "ircut_setting"        // IRCUT 工作模式(自动， 定时， 外部控制， 手动)
#define FUNCTION_IRCUT_BY_ADC_HARDWARE "ircut_by_adc" // 根据硬件光敏电阻adc值来控制IRCUT
#define FUNCTION_HTTPS "with_https"                   // 是否支持https
#define FUNCTION_H5 "with_h5"                         // 是否支持h5
#define FUNCTION_QP "with_qpsetting"                  // 是否支持VBR QP自定义设置

#define FUNCTION_IRCUT_LED_DELAY "ircut_leddelay"                // 补光延时配置，用于软光敏方式， 可以开补光灯的灵敏度
#define FUNCTION_IRCUT_LED_MANUAL_SWITCH "ircut_ledmanualswitch" // 支持手动灯光常开/常关

#define FUNCTION_PROFLE_SETTING "profile_setting"
#define FUNCTION_WDR_SETTING "wdr_setting" // DSP 数字宽动态 设置
#define FUNCTION_HDR_SETTING "hdr_setting" // SENSOR 宽动态设置
#define FUNCTION_VIDEO_MASK "video_mask"   // 视频遮挡设置
#define FUNCTION_SYSTEM_MAINTAIN "system_maitain"
#define FUNCTION_LINKDOWN_RECORD "linkdown_record"
#define FUNCTION_PPTP "pptp_support"

#define FUNCTION_AUDIO "audio_support"

#define FUCNTION_3G_EVDO "evdo_support"
#define FUCNTION_3G_WCDMA "wcdma_support"
#define FUCNTION_3G_TDSCDMA "tdscdma_support"

#define FUNCTION_LANGUAGE_ZH_CN "zh_cn"
#define FUNCTION_LANGUAGE_ZH_TW "zh_tw"
#define FUNCTION_LANGUAGE_ZH_HK "zh_hk"
#define FUNCTION_LANGUAGE_EN_US "en_us"
#define FUNCTION_LANGUAGE_RU_PY "rs_py"
#define FUNCTION_LANGUAGE_TR_TR "tr-tr"
#define FUNCTION_LANGUAGE_KO_KO "ko-ko"
#define FUNCTION_LANGUAGE_CZ_CZEKH "cz-czekh"

#define FUNCTION_SEARCH_WIFIAP "SEARCH_WIFIAP"
#define FUNCTION_LONG_TITLE "LONG_TITLE"
#define FUNCTION_TIMEZONE_HALFHOUR "timezone_halfhour"
#define FUNCTION_P2P_CFG "p2p_cfg_support"

#define FUNCTION_SUPPORT_RECORD_AJV "ajv_support"
#define FUNCTION_SUPPORT_MP4 "mp4_support"
#define FUNCTION_MD_18X22 "md_18x22"
#define FUNCTION_ONLY_18X22 "only_18x22"
#define FUNCTION_AMBAR_ENCMODE "encmode_setting"
#define FUNCTION_SUPPORT_LBR "lbr_support"
#define FUNCTION_SUPPORT_28181 "gb28181"
#define FUNCTION_SUPPORT_GAT1400 "gat1400"
#define FUNCTION_SUPPORT_IPVS "ipvs"
#define FUNCTION_SUPPORT_RTMP "rtmp"
#define FUNCTION_SUPPORT_RTMP_TIMESPAN "rtmp_timespan"
#define FUNCTION_HIK_CONFIG "hikconfig"
#define FUNCTION_COMM_ONVIF_ENABLE "commenable"
#define FUNCTION_DH_CONFIG "dhconfig"
#define FUNCTION_UNV_CONFIG "unv#"

#define FUCNTION_VOIP "voip"
#define FUNCTION_ROTATE "rotate_enable"                    // 视频旋转
#define FUNCTION_AUDIO_CAPTURE_PARAMETER "audio_aisetting" // 音频采集通道数、采样率设置
#define FUNCTION_AUDIO_AMPLIFY "audio_amplify"
#define FUNCTION_ALOWIP_SETTING "ipaddrlimit"
#define FUNCTION_OVERLAYFPS_SETTING "overlayfps" // 叠加帧率
#define FUNCTION_GAIN_SETTING "gainsetting"      // 手动增益/自动增益
#define FUNCTION_ALARMCLOCK_SETTING "alarmclock"
#define FUNCTION_ALARM_VIDEOGATE "AlarmVG"         // 拌线检测
#define FUNCTION_ALARM_VIDEOGATE_BY_PD "AlarmPdVG" // 人形检测实现拌线检测
#define FUNCTION_ALARM_REGION_AI "AlarmRegionAI"   // 区域侦测
#define FUNCTION_ISP_MODE "ispmode"
#define FUNCTION_ALARM_PD "VideoPD"
#define FUNCTION_ALARM_PD_SEMSIVITY_NEW "aiobjectsensivity"             // 智能检测灵敏度细分--20240419细分每一种算法的灵敏度
#define FUNCTION_ALARM_COVER "alarm_cover"                              // 视频遮挡报警
#define FUNCTION_FACE_FD "face_detect"                                  // 人脸检测
#define FUNCTION_FACE_FR "face_recognize"                               // 人脸匹配
#define FUNCTION_PD_RECT_2 "pd2rectconf"                                // 人形框区域框分开配置
#define FUNCTION_PD_TRACK_HUMAN "humantrack"                            // 人形跟踪
#define FUNCTION_PD_MOSIC "humanmosic"                                  // 人形马赛克
#define FUNCTION_PD_NONMotionFilter "nonMotionFilter"                   // 不动不检
#define FUNCTION_PD_TRACK_GUNBALL_SIMPLE "gunballtrack1"                // 枪球跟踪简单模式:球跟踪/枪球跟踪
#define FUNCTION_PD_TRACK_GUNBALL_ADV "gunballtrack2"                   // 枪球跟踪高级模式:球跟踪/枪球跟踪/枪球联动跟踪
#define FUNCTION_ALARM_VEHICLE_CAR "VehicleCar"                         // 车形检测-汽车
#define FUNCTION_ALARM_VEHICLE_MOTO "VehicleMoto"                       // 车形检测-摩托
#define FUNCTION_ALARM_VEHICLE_ELECTRICBICYCLE "VehicleElectricbicycle" // 车型检测-电单车
#define FUNCTION_ALARM_VEHICLE_BICYCLE "VehicleBicycle"                 // 车型检测-自行车
#define FUNCTION_ALARM_LPR "LPRecognition"                              // 车牌识别
#define FUNCTION_TIMESPAN_NEW "TimeSpanNew"                             // 新的7X24字节表示的时间段，精确到小时
#define FUNCTION_ALARM_FIRE "Fire"                                      // 火焰检测
#define FUNCTION_ALARM_GASTANK "Gastank"                                // 煤气罐检测

#define FUCTION_PD_POLYGON "pd_polygon_area"               // 人形支持多边形监控区域
#define FUCTION_PD_MADP "pd_madp"                          // mstar 的人形算法
#define FUNCTION_OSD_ANYPOSITION "OSD_ANYPOS"              // OSD任意位置
#define FUNCTION_VIDEO_CROP "video_crop"                   // 视频裁剪
#define FUNCTION_VIDEO_FORCT_ANTIFLICKER "antiflicker"     // 强制抗闪
#define FUCTION_ADVANCE_LIGHT_CONTROL "advance_light_ctrl" // 远瞻提出的灯光控制功能， 包括告警触发亮度，值守亮度
#define FUCTION_IO_OUTPUT_SET "io_output_set"              // IO 输出配置
#define FUNCTION_ALARM_TEMP_HUMIDITY "temp_humidity"       // 温湿度检测  temperature  humidity
// #define FUNCTION_ALARM_TEMPERATURE	"temperature_set"
// #define FUNCTION_ALARM_HUMIDITY		"humidity_set"

#define FUNCTION_ALARM_TOXIC_GAS "toxic_gas" // 有毒气体

#define FUCTION_ONE_OUTPUT "one_output"
#define FUCTION_TWO_OUTPUT "two_output"
#define FUCTION_THREE_OUTPUT "three_output"
#define FUCTION_FOUR_OUTPUT "four_output"

#define FUCTION_ONE_INPUT "one_input"
#define FUCTION_TWO_INPUT "two_input"
#define FUCTION_THREE_INPUT "three_input"
#define FUCTION_FOUR_INPUT "four_input"

#define FUNCTION_VIDEOMASK_ONESET "ONEVIDEOMASK" // 主子码流使用同一套隐私遮挡，不需要分开配置
#define FUNCTION_MULTICAST "multicast"
#define FUNCTION_VIDEOSHUTTER "VideoShutter" // 电子快门设置

#define FUNCTION_MAC_MODIFY "ModifyMac"
#define FUNCTION_AUDIO_ALARM "AudioAlarm"

// 海思编码支持宽动态、去雾功能，参数范围0-255
#define FUNCTION_HISCON_ENCMODE "hisconenc"

#define FUNCTION_P2P_CONFIG "p2p_config"
#define FUNCTION_P2P_DANALE "p2p_danale"
#define FUNCTION_P2P_AIOT "p2p_aiot"
#define FUNCTION_P2P_ANKO "p2p_anko"
#define FUNCTION_P2P_GOOLINK "p2p_goolink"
#define FUNCTION_P2P_SKYWORTH "p2p_skyworth"
#define FUNCTION_P2P_AC18PRO "p2p_ac18pro"
#define FUNCTION_P2P_TENCENTIOT "p2p_tencent"
#define FUNCTION_P2P_ISMART "p2p_ismart"
#define FUNCTION_P2P_QQ "p2p_qq"
#define FUNCTION_P2P_EYEPLUS "p2p_eyeplus"

#define FUNCTION_PRE_RECORD "pre_record"
#define FUNCTION_UPLOAD_SOFTCRYPT "crypt_upload"
#define FUNCTION_OSD_GB28181 "osd_gb"                     // 国标字体大小
#define FUNCTION_AF_VERSION "af_setting"                  // AF设置
#define FUNCTION_AF_Coordinate "af_coordinate"            // 配置坐标数据写入串口
#define FUNCTION_AUDIO_8M_Repartition "audio_Repartition" // AF设置

#define FUNCTION_OTA_SUPPORT "ota_enable"

#define FUNCTION_PTZ_ALL_CTRL "ptz_all_ctrl"
#define FUNCTION_PTZ_ZOOM "ptz_zoom"
#define FUNCTION_PTZ_FOCUS "ptz_focus"
#define FUNCTION_PTZ_IRIS "ptz_iris"
#define FUNCTION_PTZ_2_DIRECTION "ptz_2_direction"
#define FUNCTION_PTZ_4_DIRECTION "ptz_4_direction"
#define FUNCTION_PTZ_8_DIRECTION "ptz_8_direction"
#define FUNCTION_HIGHCTRL_PTZ "high_ctrl_ptz"
#define FUNCTION_PTZ_NO_SCAN "ptz_no_scan" // 不支持巡航和线扫
#define FUNCTION_SMART_265 "smart265"

#define FUNCTION_PTZ_ONLY_DIGIT_ZOOM "ptz_only_digit_zoom" // 只支持数字变焦

#define FUNCTION_PTZ_CRUISE "ptz_cruise"    // 支持云台巡航
#define FUUNCTION_PTZ_AB_SCAN "ptz_ab_scan" // 支持云台AB线扫

#define FUNCTION_SCARE_OFF "scare_off"
#define FUNCTION_BE_SET_MTU "be_set_mtu" // 支持设置mtu
#define FUNCTION_ELE_ZOOM "ele_zoom"     // 支持电子放大，用于多目摄像头
#define FUNCTION_ZOOM_TRACK "zoom_track" // 变倍跟踪

#define FUNCTION_POWER_DISPLAY "device_power_display"    // 电量显示
#define FUNCTION_LOW_POWER "low_power_support"           // 省电模式
#define FUNCTION_PRIVACY_PROTECTION "privacy_protection" // 一键遮挡
#define FUNCTION_BRIGHTNESS_MODE1 "brightness_mode1"     // 亮度模式1， 开启补光延迟列表显示会不一样， 目前只K45支持

#define FUNCTION_AF_PROTOCOL_5 "af_protocol_5"       // 威特迪云台控制方式
#define FUNCTION_AF_PROTOCOL_4 "af_protocol_4"       // 支持机芯，支持高级云台设置
#define FUNCTION_ACTIVE_CALL "active_call"           // 主动呼叫功能
#define FUNCTION_TWOLENS_STICH "twolens_stitch"      // 双目拼接
#define FUNCTION_ARMING_DAYNIGHT "arming_daynight"   // 布防设置为禁止/全天/白天/夜晚，具备该能力集时移动侦测/人形/越界/区域支持红蓝警灯/报警推送设置日夜布防
#define FUNCTION_ARMING_TOTALSWITCH "alarm_arming"   // 报警设置启用与布防设置分开，增加布防总开关
#define FUNCTION_ARMING_BYTIME "arming_bytime"       // 布防设置支持时间段
#define FUNCTION_ARMINGAUDIO_DESC "arming_audiodesc" // 支持警戒声音描述
#define FUNCTION_IOOUT_ARMING "ioout_arming"         // IO输出支持布防时间段设置

#define FUNCTION_DEVICE_REPORT "device_report"              // 具备上报P2PID/4G ICCID等信息到后台能力
#define FUNCTION_CREATE_TIMELAPSE_RECORD "create_timelapse" // 具备生成缩时录像的能力
#define FUNCTION_CREATE_EXPORT_RECORD "export_record"       // 具备生成导出录像的能力
#define FUNCTION_RESTORE_RETAIN_PART "part_restore"         // 具备在恢复出厂时保留部分设置能力

#define FUNCTION_PTZ_DZOOMSETTING "dzoomsetting"           // 支持设置数字变倍
#define FUNCTION_4G_SWITCH_2CARD "dual4gswitch"            // 支持4G双卡切换
#define FUNCTION_DDNS "ddns_cfg"                           // 支持DDNS配置
#define FUNCTION_FISHEYE_COVER "fisheye_cover"             // 支持鱼眼遮盖
#define FUNCTION_PTZ_FIRMWARE_UPDATE "ptz_firmware_update" // 支持云台固件在线升级
#define FUNCTION_EXTCHANNEL1_SETTING "ExtChannel1"         // 扩展1个通道设置
#define FUNCTION_EXTCHANNEL2_SETTING "ExtChannel2"         // 扩展2个通道设置
#define FUNCTION_P2P_PRIVATE_CMD "P2PNEWROOT"              // P2P私有协议新节点
#define FUNCTION_DISABLE_MD_WHEN_HD "hd_disablemd"         // 支持启用AI检测时禁用MD的配置
#define FUNCTION_BATTERY_MODE "battery_mode"               // 电池低功耗模式
#define FUNCTION_4G_CONFIG "g4_config"                     // 支持4G网络模式配置
#define FUNCTION_PTZ_SCAN_CFG "ptz_scan_cfg"               // 支持设置云台线扫和巡航速度等设置
#define FUNCTION_VIDEO_QOS "video_qos"                     // 支持配置视频服务质量
#define FUNCTION_AUDIO_PROMPT "audioprompt"                // 支持配置工作提示音
#define FUNCTION_HIGH_SPEED_PTZ "high_speed_ptz"
#define FUNCTION_PTZ_RE_CALIBRAT "ptz_re_calibrat" // 抢球云台重校准
#define FUNCTION_AOV_SUPPORT "aov_support"
#define FUNCTION_RECORD_ALARMLIST_SUPPORT "record_alarmlist_support" // 录像告警列表
#define FUNCTION_FIXIP "fixipcfg"                                    // 支持MAC/IPIP配置防篡改

#define FUNCTION_FALLOCATE_REC "sd_fallocate_rec"    // 预分配卡录能力集
#define FUNCTION_PTZ_AUTOHOME_CFG "ptz_homecfg"      // 自动归位/自动校准/低电控制
#define FUNCTION_ALARM_PUSH_CLOUD "alarm_push_cloud" // 告警消息推送到云OSS上
#define FUNCTION_CLOUD_CTYUN "ctyun_cloud"           // 天翼云
#define FUNCTION_OSD_4G_CFG "osd_4g"
#define FUNCTION_GUN_DOME_SUPPORT "gun_dome_support"
#define FUNCTION_PTZ_LOWPOWER_CTRL "ptz_lowpower"   // 低电控制
#define FUCNTION_PTZ_AUTOHOME_CTRL "ptz_autohome"   // 自动归位
#define FUNCTION_PTZ_AUTOCALIBRATION "ptz_autocali" // 自动校准

#define FUNCTION_PRECOMM_PB_SUPPORT "connpre" // 支持预连接回放
#define FUNCTION_LOCATION "location"
#define FUNCTION_RIDGEOS_CONFIG "ridgeOS_config"
#define FUNCTION_PROPERTIES_CLOUD "prop#" // 属性写云存储
#define FUNCTION_LED_CFG "led_new"        // 开关灯灵敏度高中低三档

#endif
