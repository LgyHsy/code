#include <stdio.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_time.h"
#include "anj_mw_hwctrl.h"
#include "anj_comm.h"
#include "anj_module.h"
#include "eventhub.h"

#include "anj_config.h"
#include "anj_osd.h"
#include "anj_audio.h"

#define ADC_WINDOW_COUNT    50
#define ADC_START_CAP_COUNT (ADC_WINDOW_COUNT/10)

#define BATTERY_NORMAL_POWER_THRESHOLD 20   // 正常电量阈值
#define BATTERY_LOW_POWER_THRESHOLD 15      // 低电阈值

typedef enum
{
    BATTERY_CAP_NORMAL = 0,
    BATTERY_CAP_LESS_15
}BAT_WORK_CAP_E;

typedef struct
{
    int bat_cur_dvfs;               // 动态电压频率
    int bat_cur_cap;                // 当前电量
    int bat_last_cap;               // 上一次检测电量
    BAT_WORK_CAP_E e_bat_work_cap;  // 工作模式
    int *p_bat_cap_form;            // 电量表格
    

    int bat_charge_status;          // 充电状态
    
}BatteryPowerInfo_t;

static pthread_mutex_t s_stBatteryMutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_stBatteryThread = {0};

static BatteryPowerInfo_t s_stBatteryInfo;

static int s_adc_value_index = 0;
static unsigned int s_adc_value_window[ADC_WINDOW_COUNT] = {0};

int bat_cap_1[]=
{
    12526,12399,12336,12295,12265,12241,12221,12204,12191,12179,
    12168,12159,12151,12144,12137,12131,12127,12122,12118,12114,
    12111,12107,12105,12102,12100,12096,12093,12090,12088,12085,
    12082,12079,12074,12070,12063,12056,12047,12037,12025,12011,
    11999,11987,11977,11969,11962,11954,11946,11939,11931,11922,
    11912,11902,11890,11876,11862,11845,11825,11808,11789,11771,
    11752,11735,11718,11704,11689,11675,11664,11652,11642,11633,
    11624,11615,11607,11599,11590,11580,11569,11556,11542,11527,
    11513,11498,11483,11465,11445,11422,11398,11374,11350,11325,
    11305,11290,11278,11266,11253,11238,11218,11182,11094,10953,10772,10529
};

int bat_cap_2[]=
{
    4140,4120,4100,4080,4060,4040,4020,4000,3980,3970,
    3960,3950,3940,3935,3931,3926,3922,3917,3913,3908,
    3904,3899,3896,3892,3889,3885,3882,3878,3875,3871,
    3868,3864,3861,3857,3854,3850,3847,3843,3840,3836,
    3833,3829,3826,3822,3819,3815,3812,3808,3804,3800,
    3798,3796,3794,3792,3790,3788,3786,3784,3782,3780,
    3774,3768,3762,3756,3750,3744,3738,3732,3726,3720,
    3716,3712,3708,3704,3700,3696,3692,3688,3684,3680,
    3666,3652,3638,3624,3610,3596,3582,3568,3554,3540,
    3486,3432,3378,3324,3270,3216,3162,3108,3054,3000,3000,3000
};

int bat_cap_3[]=
{
    41800,41580,41360,41140,40920,40700,40480,40040,39820,39600,
    39560,39520,39480,39440,39400,39360,39320,39280,39240,39200,
    39180,39160,39140,39120,39100,39080,39060,39040,39020,39000,
    38940,38880,38820,38760,38700,38640,38580,38520,38460,38400,
    38360,38320,38280,38240,38200,38160,38120,38080,38040,38000,
    37980,37960,37940,37920,37900,37880,37860,37840,37820,37800,
    37740,37680,37620,37560,37500,37440,37380,37320,37260,37200,
    37160,37120,37080,37040,37000,36960,36920,36880,36840,36800,
    36660,36520,36380,36240,36100,35960,35820,35680,35540,35400,
    34860,34320,33780,33240,32700,32160,31620,31080,30540,30000,30000,30000
};


int anj_battery_calculate_dvfs(int adc_value)
{
    int dvfs_value = 3300 * adc_value * 1100 / 1023 / 100 + 250;
    return dvfs_value;
}

int anj_battery_transform_capacity(int dvfs_value)
{
    int bat_cap = 0;
    int i = 0;

    if (dvfs_value >= s_stBatteryInfo.p_bat_cap_form[0])
    {
        bat_cap = 100;
    }
    else if (dvfs_value <= s_stBatteryInfo.p_bat_cap_form[101])
    {
        bat_cap = 0;
    }
    else
    {
        for(i = 100; i > 0; i--)
        {
            if (dvfs_value <= s_stBatteryInfo.p_bat_cap_form[i])
            {
                break;
            }
        }

        bat_cap = 100 - i - 1;
        bat_cap = (bat_cap < 0) ? 0 : bat_cap;
    }

    return bat_cap;
}

// 上电时采样，填充满滑动滤波窗口
int anj_battery_adc_window_init()
{
    int i = 0;
    int adc_sum = 0;
    int adc_average = 0;
    int adc_init_window[ADC_START_CAP_COUNT] = {0};

    for (i = 0; i < ADC_START_CAP_COUNT; i++)
    {
        adc_init_window[i] = anj_mw_hwctrl_batteryadc_get();
        usleep(100 * 1000);
    }

    s_adc_value_index = 0;
    for (i = 0; i < ADC_WINDOW_COUNT; i++)
    {
        int sample_idx = i / (ADC_WINDOW_COUNT / ADC_START_CAP_COUNT);
        s_adc_value_window[i] = adc_init_window[sample_idx];
        adc_sum += s_adc_value_window[i];
    }

    adc_average = adc_sum / ADC_WINDOW_COUNT;

    return adc_average;
}

// 获取平均后的adc值
int anj_battery_adc_average_value_get(int adc_value)
{
    int i = 0;
    int adc_sum = 0;
    int adc_average = 0;

    if (s_adc_value_index >= ADC_WINDOW_COUNT)
    {
        s_adc_value_index = 0;
    }

    s_adc_value_window[s_adc_value_index] = adc_value;
    s_adc_value_index++;

    for(i = 0; i < ADC_WINDOW_COUNT; i++)
    {
        adc_sum += s_adc_value_window[i];
    }

    adc_average = adc_sum / ADC_WINDOW_COUNT;

    return adc_average;
}

void anj_battery_osd_update(int dvfs_value, int bat_cap, int charge_stat)
{
    SystemConfig *pSystemConfig = getSystemConfig();
    char bat_stat_text_uft8[48] = {0};

    float fdvfs_value = dvfs_value * 0.001;

    osd_custom_content_s osdBatteryCustom = {0};
    osdBatteryCustom.custom_show = 1;
    osdBatteryCustom.custom_x = 0;
    osdBatteryCustom.custom_y = 1;
    osdBatteryCustom.custom_location = POSITION_TYPE_BY_FOUR_CORNER;

    if (charge_stat)
    {
        if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
        {
            sprintf(bat_stat_text_uft8, "充电中");
        }
        else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
        {
            sprintf(bat_stat_text_uft8, "充電中");
        }
        else
        {
            sprintf(bat_stat_text_uft8, "Charging");
        }

        if (bat_cap == 100)
        {
            bat_cap = 99;
        }
    }
    else
    {
        if (bat_cap == 100)
        {
            if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
            {
                sprintf(bat_stat_text_uft8, "已充满");
            }
            else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
            {
                sprintf(bat_stat_text_uft8, "已充滿");
            }
            else
            {
                sprintf(bat_stat_text_uft8, "Fully Charged");
            }
        }
        else
        {
            if (OSD_SHOW_BAT_VOLT)
            {
                if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
                {
                    sprintf(bat_stat_text_uft8, "电压");
                }
                else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
                {
                    sprintf(bat_stat_text_uft8, "電壓");
                }
                else
                {
                    sprintf(bat_stat_text_uft8, "Voltage");
                }
            }
            else
            {
                if (strcmp(pSystemConfig->miscCfg.language, "zh_cn") == 0)
                {
                    sprintf(bat_stat_text_uft8, "电量");
                }
                else if (strcmp(pSystemConfig->miscCfg.language, "zh_tw") == 0)
                {
                    sprintf(bat_stat_text_uft8, "電量");
                }
                else
                {
                    sprintf(bat_stat_text_uft8, "Battery");
                }
            }
        }
    }

    if (OSD_SHOW_BAT_VOLT)
    {
        snprintf(osdBatteryCustom.overlayStr, sizeof(osdBatteryCustom.overlayStr), " %s=%2.1fV", 
            bat_stat_text_uft8, fdvfs_value);
    }
    else
    {
        snprintf(osdBatteryCustom.overlayStr, sizeof(osdBatteryCustom.overlayStr), " %s=%d%%", 
            bat_stat_text_uft8, bat_cap);
    }

    anj_osd_battery_set(&osdBatteryCustom);
}

void anj_battery_audio_play(BAT_WORK_CAP_E stat)
{
    audioplay_info audio_info = {0};
    if (stat == BATTERY_CAP_LESS_15)
    {
        snprintf(audio_info.pAudioFile, sizeof(audio_info.pAudioFile), AJ_APP_PATH"mp3/ch/s16k_a_lowbattery_warn.mp3");
        audio_info.playtimes = 1;
        audio_info.playaction = AUDIO_PLAY_ACTION_WAIT_PREV;
        audio_info.priority = AUDIO_PLAY_PRIORITY_MAX;
        audio_info.encodeType = MEDIA_CODEC_AUDIO_MP3;
        anj_audio_play_file(&audio_info);
    }
}

int anj_battery_manage_thread(void *ctx, int *bStart)
{
    anj_mw_hwctrl_batteryadc_init();
    usleep(500 * 1000);

    int adc_value = 0;
    int adc_average = 0;
    EventResult event_result = {0};

    // 等待电机不转动时，填充滑动滤波窗口
    while(bStart && *bStart)
    {
        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_MOVE_STATUS, &event_result, NULL);
        if(event_result.ret == 0)
        {
            adc_average = anj_battery_adc_window_init();
            break;
        }

        usleep(200 * 1000); 
    }

    // 采集完直接计算一次电量，防止电机继续动一直获取不到电量
    s_stBatteryInfo.bat_charge_status = 0;
    s_stBatteryInfo.bat_cur_dvfs = anj_battery_calculate_dvfs(adc_average);
    s_stBatteryInfo.bat_cur_cap = anj_battery_transform_capacity(s_stBatteryInfo.bat_cur_dvfs);
    s_stBatteryInfo.bat_last_cap = s_stBatteryInfo.bat_cur_cap;
    if (s_stBatteryInfo.bat_cur_cap <= BATTERY_LOW_POWER_THRESHOLD)
    {
        s_stBatteryInfo.e_bat_work_cap = BATTERY_CAP_LESS_15;
        anj_battery_audio_play(s_stBatteryInfo.e_bat_work_cap);
    }
    anj_battery_osd_update(s_stBatteryInfo.bat_cur_dvfs, s_stBatteryInfo.bat_cur_cap, s_stBatteryInfo.bat_charge_status);
    __INFO("battery init dvfs:%d, cap:%d, charge stat:%d\n", s_stBatteryInfo.bat_cur_dvfs, s_stBatteryInfo.bat_cur_cap, s_stBatteryInfo.bat_charge_status);


    while(bStart && *bStart)
    {
        anj_mutex_lock(&s_stBatteryMutex);

        eventhub_publish(EVENTHUB_CLASS_CTRL, EVENTHUB_PTZ_MOVE_STATUS, &event_result, NULL);
        if (event_result.ret == 1)
        {
            anj_mutex_unlock(&s_stBatteryMutex);
            usleep(500 * 1000);
            continue;
        }

        adc_value = anj_mw_hwctrl_batteryadc_get();
        adc_average = anj_battery_adc_average_value_get(adc_value);

        s_stBatteryInfo.bat_cur_dvfs = anj_battery_calculate_dvfs(adc_average);
        s_stBatteryInfo.bat_cur_cap = anj_battery_transform_capacity(s_stBatteryInfo.bat_cur_dvfs);

        // 目前只有V10等带mcu的产品能检测充电状态
        s_stBatteryInfo.bat_charge_status = 0;

        if (s_stBatteryInfo.bat_cur_cap != s_stBatteryInfo.bat_last_cap)
        {
            __INFO("battery cap update! cur:%d, last:%d\n", s_stBatteryInfo.bat_cur_cap, s_stBatteryInfo.bat_last_cap);
            anj_battery_osd_update(s_stBatteryInfo.bat_cur_dvfs, s_stBatteryInfo.bat_cur_cap, s_stBatteryInfo.bat_charge_status);
            s_stBatteryInfo.bat_last_cap = s_stBatteryInfo.bat_cur_cap;
        }

        anj_mutex_unlock(&s_stBatteryMutex);
        usleep(500 * 1000);
    }

    return 0;
}

void anj_battery_info_init()
{
    memset(&s_stBatteryInfo, 0, sizeof(BatteryPowerInfo_t));

    s_stBatteryInfo.p_bat_cap_form = bat_cap_1;
    s_stBatteryInfo.e_bat_work_cap = BATTERY_CAP_NORMAL;
    s_stBatteryInfo.bat_cur_cap = 0;
}

static void anj_battery_cap_get(EventResult *event_result, void *data)
{
    if (event_result && data)
    {
        int *value = (int *)data;
        anj_mutex_lock(&s_stBatteryMutex);
        *value = s_stBatteryInfo.bat_cur_cap;
        anj_mutex_unlock(&s_stBatteryMutex);
        event_result->ret = 0;
    }
}

static void anj_battery_charge_get(EventResult *event_result, void *data)
{
    if (event_result && data)
    {
        int *value = (int *)data;
        anj_mutex_lock(&s_stBatteryMutex);
        *value = s_stBatteryInfo.bat_charge_status;
        anj_mutex_unlock(&s_stBatteryMutex);
        event_result->ret = 0;
    }
}

int anj_battery_init()
{
    anj_battery_info_init();

    strncpy(s_stBatteryThread.iThreadName, "anj_battery_manage_thread", sizeof(s_stBatteryThread.iThreadName) - 1);
    s_stBatteryThread.iThreadjob.func = anj_battery_manage_thread;
    s_stBatteryThread.iThreadjob.ctx = &s_stBatteryThread;
    anj_thread_task_create(&s_stBatteryThread);

    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_BATTERY_CAP_GET, anj_battery_cap_get);
    eventhub_subscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_BATTERY_CHARGE_GET, anj_battery_charge_get);

    return 0;
}

int anj_battery_uninit()
{
    anj_thread_task_destroy(&s_stBatteryThread, -1);
    return 0;
}

REGISTER_MODULE(anj_battery, MODULE_PRIORITY_BATTERY);

