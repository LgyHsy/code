#ifndef __ANJ_AUDIO_H__
#define __ANJ_AUDIO_H__

#include "media_util.h"
#include "anj_mw_file.h"

#ifdef __cplusplus
extern "C"
{
#endif
#define ANJ_MP3_DEFAULT_PATH            "/opt/ch/mp3"
#define ANJ_MP3_DEVICE_PATH             ANJ_MP3_DEFAULT_PATH "/device"
#define ANJ_MP3_4G_PATH                 ANJ_MP3_DEFAULT_PATH "/4g"
#define ANJ_MP3_NETWORK_PATH            ANJ_MP3_DEFAULT_PATH "/network"
#define ANJ_MP3_BIND_PATH               ANJ_MP3_DEFAULT_PATH "/bind"
#define ANJ_MP3_ALARM_PATH              ANJ_MP3_DEFAULT_PATH "/alarm"
#define ANJ_MP3_SDCARD_PATH             ANJ_MP3_DEFAULT_PATH "/sdcard"
#define ANJ_MP3_OTA_PATH                ANJ_MP3_DEFAULT_PATH "/ota"
#define ANJ_MP3_COMM_PATH               ANJ_MP3_DEFAULT_PATH "/comm"

#define ANJ_MP3_DEVICE_STARTUP          "startup.mp3"
#define ANJ_MP3_DEVICE_UPDATED          "updating_finished.mp3"
#define ANJ_MP3_DEVICE_START_UPDATE     "updating started.mp3"
#define ANJ_MP3_DEVICE_START            "s16k_a_device_started.mp3"
#define ANJ_MP3_BIND_SUCCESS            "s16k_a_bind_success.mp3"
#define ANJ_MP3_BIND_FAIL               "s16k_a_use_lan_search.mp3"
#define ANJ_MP3_CONNECT_FAIL            "s16k_a_connect_fail.mp3"
#define ANJ_MP3_CONNECTING_NET          "s16k_a_connecting_network.mp3"
#define ANJ_MP3_CONFIG_NET              "s16k_a_start_config_network.mp3"
#define ANJ_MP3_WAIT_CONFIG_NET         "s16k_a_waiting_config_network.mp3"
#define ANJ_MP3_CONFIG_INVALID          "s16k_a_invalid.mp3"
#define ANJ_MP3_NETWORK_CONNECTED       "network_connected.mp3"
#define ANJ_MP3_NETWORK_DISCONNECTED    "network_disconnected.mp3"
#define ANJ_MP3_CONNECT_NET_SUCCESS     "s16k_a_internet_connect_success.mp3"
#define ANJ_MP3_CONNECT_NET_FAIL        "s16k_a_abnormal_internet_connection.mp3"
#define ANJ_MP3_WIFI_CONFIG_RECEIVED    "s16k_a_config_msg_received.mp3"
#define ANJ_MP3_WIFI_PASSWORD_ERROR     "s16k_a_password_incorrect.mp3"
#define ANJ_MP3_WIFI_CONNECTING         "s16k_a_wifi_connecting.mp3"
#define ANJ_MP3_WIFI_CONNECT_FAIL       "s16k_a_wifi_connect_fail.mp3"
#define ANJ_MP3_CONNECT_WIRE_FAIL       "s16k_a_wire_connect_fail.mp3"
#define ANJ_MP3_CONNECTING_WIRE         "s16k_a_wire_connecting.mp3"
#define ANJ_MP3_CONNECTED_WIRE          "s16k_a_wire_connect_success.mp3"
#define ANJ_MP3_SCAN_QRCODE             "s16k_a_please_scan_code_to_add.mp3"
#define ANJ_MP3_CHECK_DEVICE_STATUS     "s16k_a_please_check_device_status.mp3"
#define ANJ_MP3_SIMCARD_INVALID         "s16k_a_invalid_simcard.mp3"
#define ANJ_MP3_SIGNAL_WEAK             "s16k_a_signal_strength_less_than_40.mp3"
#define ANJ_MP3_SIGNAL_STRENGTH         "s16k_a_signal_strength.mp3"
#define ANJ_MP3_TELECOM_ERROR           "telecom_error.mp3"
#define ANJ_MP3_MOBILE_ERROR            "mobile_error.mp3"
#define ANJ_MP3_FACTORY_RESTORE         "s16k_a_factory_restore.mp3"
#define ANJ_MP3_PERCENT                 "s16k_a_percent_%d.mp3"
#define ANJ_MP3_NO_4G                   "no4g.mp3"
#define ANJ_MP3_AP_MODE                 "switch_ap_mode.mp3"

#define ANJ_MP3_SD_FORMAT                   "sd_format_needed.mp3"
#define ANJ_MP3_SD_ABNORMAL                 "sd_abnormal.mp3"
#define ANJ_MP3_SD_INSERT                   "sd_inserted.mp3"
#define ANJ_MP3_SD_REMOVE                   "sd_removed.mp3"

#define ANJ_MP3_SIM1_ERROR              "sim1_error.mp3"
#define ANJ_MP3_SIM2_ERROR              "sim2_error.mp3"
#define ANJ_MP3_DI_DI                   "s16k_a_di.mp3"

typedef enum
{
    AUDIO_PLAY_PRIORITY_MAX,
    AUDIO_PLAY_PRIORITY_1,
    AUDIO_PLAY_PRIORITY_2,
    AUDIO_PLAY_PRIORITY_3,
    AUDIO_PLAY_PRIORITY_4,
    AUDIO_PLAY_PRIORITY_5,
    AUDIO_PLAY_PRIORITY_6,
    AUDIO_PLAY_PRIORITY_7,
    AUDIO_PLAY_PRIORITY_8,
    AUDIO_PLAY_PRIORITY_9,
    AUDIO_PLAY_PRIORITY_10,
}PLAY_AUDIO_PRIORITY;

typedef enum
{
    AUDIO_FILE_PLAY_READY = 0,
    AUDIO_FILE_PLAY_PLAYING,
}AudioFilePlayStatus;

typedef struct
{
    char pAudioFile[64];
    media_codec_type_e encodeType;
    int priority;       // 优先级
    int playtimes;      // 播放次数
    int playaction;     // 播放动作 AUDIO_PLAY_ACTION
} audioplay_info;

typedef struct
{
    unsigned long long playtime;
    int bTalk; // 0: 播放提示音，1: 对讲播放
}AudioPlayInfo;

typedef struct
{
    char *buff;             // 音频aiao测试buff，采集2.5s的数据，最大40k
    int   buf_pos;          // buf偏移
    int   startup;          // 开始采集flag
    int   finish_status;    // 采集完成状态
    unsigned long long start_pts;       // 开始采集时间
}AudioAiAoTestInfo_t;

void anj_audio_restart();
int anj_audio_ai_volume_set(int volume, int amplify);
int anj_audio_ao_volume_set(int volume);
int anj_audio_play_file(audioplay_info *pstAudioInfo);
void anj_audio_play_data(char *data, int data_len, int bTalk, media_codec_type_e codec_type, int samplerate);
void anj_audio_talk_reset(void);

int anj_audio_ai_mute_set(int enable);
int anj_audio_ao_play_file_status_get();

void anj_audio_aiao_test_start();
void anj_audio_aiao_test_stop();
AudioAiAoTestInfo_t *anj_audio_aiao_test_info_get();

void anj_audio_prompt_play(char *filepath, char *filename, int cnt);
void anj_audio_play_file_stop(void);

int anj_audio_mp3_file_list_query(file_query_result *pstFileQueryResult, int skip_count, int page_size);

#ifdef __cplusplus
}
#endif

#endif
