#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "anj_mw_log.h"
#include "anj_mw_thread.h"
#include "anj_mw_time.h"
#include "anj_mw_hwctrl.h"
#include "anj_module.h"
#include "anj_config.h"
#include "anj_ispctl.h"
#include "anj_osd.h"
#include "anj_sysctl.h"

#include "function_list.h"
#include "eventhub.h"
#include "alarm_link.h"

#include "anj_alarm.h"


#define ALARM_EVENT_END     0
#define ALARM_EVENT_BEGIN   1

#define ALARM_DISAPPEAR_TIMEVAL     (5*1000)             // 5s没有触发认为消失
#define ALARM_LINK_TIMEVAL          (5*1000)             // 联动间隔最少5s
#define ALARM_VG_TRIGGER_TIMEVAL    (3*1000)            // VG触发间隔3s
#define ALARM_SD_SPACELOW_TIMEVAL   (60 * 60 * 1000)    // 磁盘容量低间隔1h

#define ALARM_PD_NUM        5       // pd包括：ALARM_AI_PD、ALARM_AI_VEHICLE_CAR、ALARM_AI_VEHICLE_MOTO、ALARM_AI_VEHICLE_BICYCLE，

typedef struct
{
    unsigned long long last_link_time;      // 上一次联动时间，满足联动间隔才能执行联动
    unsigned long long trig_time;           // 报警触发时间，一直触发一直更新，基于这个时间5s后发送报警结束事件
    unsigned char event_status;             // 事件状态，触发时为BEGIN，结束设置为END
}AlarmEventInfo_t;


typedef struct
{
    AlarmEventInfo_t stAiEvent[ANJ_CAMERA_MAX_NUMS][ALARM_AI_MAX];  // AI事件
    AlarmEventInfo_t stMotion[ANJ_CAMERA_MAX_NUMS];                 // 移动侦测
    AlarmEventInfo_t stAudioLsa;        // lsa高分贝
    AlarmEventInfo_t stAudioCry;        // baby cry
}AnjAlarmInfo_t;


static anj_thread_s s_stAlarmTimerThread;       // 报警计时线程
static anj_thread_s s_stAlarminMonitorThread;
static int s_alarmin_last_state[MAX_ALARMCHANNEL_COUNT + 1]; /* index by chn 1..4，下标 0 不用 */
static AnjAlarmInfo_t s_stAnjAlarmInfo;
static pthread_mutex_t s_AlarmMutex = PTHREAD_MUTEX_INITIALIZER;

static int s_stAlarmInit = 0;

static int anj_alarm_check_disappear(unsigned long long uLastTime, unsigned long long uNowTime, int iInterval)
{
    int disappear_flag = 0;

    if (uNowTime < uLastTime)
    {
        __ERR("alarm time error! alarm trigger time:%llu later now:%llu\n", uLastTime, uNowTime);
        disappear_flag = 1;
    }
    else if (uLastTime > 0 && uNowTime >= uLastTime + iInterval)
    {
        __INFO("alarm time reached interval:%d! alarm trigger time:%llu, now:%llu\n", iInterval, uLastTime, uNowTime);
        disappear_flag = 1;
    }

    return disappear_flag;
}


//32bit: bit0: 汽车 bit1: 摩托车 bit2:电单车 bit3:自行车 bit4: 人形 ---  11011
static int anj_alarm_parse_detect_type_enable(int detectType, 
    int* isCarEnable, int* isMotorcycleEnable, int* isBicycle, int* isHumanEnable)
{
    if(detectType & (1 << AI_TYPE_BIT_CAR))
    {
        *isCarEnable = 1;
    }
    else
    {
        *isCarEnable = 0;
    }

    if(detectType & (1 << AI_TYPE_BIT_MOTO))
    {
        *isMotorcycleEnable = 1;
    }
    else
    {
        *isMotorcycleEnable = 0;
    }

    if(detectType & (1 << AI_TYPE_BIT_BICYCLE))
    {
        *isBicycle = 1;
    }
    else
    {
        *isBicycle = 0;
    }

    if(detectType & (1 << AI_TYPE_BIT_HUMAN))
    {
        *isHumanEnable = 1;
    }
    else
    {
        *isHumanEnable = 0;
    }

    return 0;
}

void init_zero_string(char *buf, int size, int zero_count)
{
    int count = (zero_count < size - 1) ? zero_count : size - 1;
    memset(buf, '0', count);
    buf[count] = '\0';
}

int anj_alarm_motion_block_valid_check(const MotionDetectAlarm *pConfig)
{
#if 0
    int xIndex, yIndex;
    int block_x	= pConfig->blockCount >> 16;	
    int block_y	= pConfig->blockCount & 0xffff;
    int bBlockCfgValid = 0;

    int nMaxY = block_y < MD_MAX_GRID_ROW ? block_y : MD_MAX_GRID_ROW;
    int nMaxX = block_x < MD_MAX_GRID_COL ? block_x : MD_MAX_GRID_COL;
    for(yIndex = 0; yIndex < nMaxY; yIndex++)
    {
        for( xIndex = 0; xIndex < nMaxX ; xIndex++)
        {
            char cType = pConfig->blockCfg[yIndex*nMaxX+xIndex];
            if(cType == '1')
            {
                bBlockCfgValid = 1;
            }
        }
    }
#else
    int bBlockCfgValid = 1;
    //int block_x = pConfig->blockCount >> 16;	
    //int block_y = pConfig->blockCount & 0xffff;
    char buf_1X1[8] = {0};
    char buf_2X2[8] = {0};
    char buf_3X2[16] = {0};
    char buf_3X3[16] = {0};
    char buf_4X3[32] = {0};
    char buf_4X4[32] = {0};
    char buf_8X8[128] = {0};
    char buf_16X16[512] = {0};
    char buf_22X18[512] = {0};

    init_zero_string(buf_1X1, sizeof(buf_1X1), 1);
    init_zero_string(buf_2X2, sizeof(buf_2X2), 4);
    init_zero_string(buf_3X2, sizeof(buf_3X2), 6);
    init_zero_string(buf_3X3, sizeof(buf_3X3), 9);
    init_zero_string(buf_4X3, sizeof(buf_4X3), 12);
    init_zero_string(buf_4X4, sizeof(buf_4X4), 16);
    init_zero_string(buf_8X8, sizeof(buf_8X8), 64);
    init_zero_string(buf_16X16, sizeof(buf_16X16), 256);
    init_zero_string(buf_22X18, sizeof(buf_22X18), 396);

    if(strcmp(pConfig->blockCfg, buf_1X1) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_2X2) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_3X2) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_3X3) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_4X3) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_4X4) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_8X8) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_16X16) == 0)
    {
        bBlockCfgValid = 0;
    }
    else if(strcmp(pConfig->blockCfg, buf_22X18) == 0)
    {
        bBlockCfgValid = 0;
    }
#endif

    return bBlockCfgValid;
}

int anj_alarm_ai_detect_trigger(int camera, int class_id, int new_alarm)
{
    int iRet = 0;
    char smart_description[64] = {0};

    __INFO("trigger pd alarm class_id:%d\n", class_id);

    if (ALARM_PD_ID_HUMAN == class_id)          //人形
    {
        snprintf(smart_description, sizeof(smart_description), "video Human shape detected.");
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_PD, new_alarm, smart_description, NULL);
    }
    else if (ALARM_PD_ID_BICYCLE == class_id)     //单车
    {
        snprintf(smart_description, sizeof(smart_description), "video Bicycle shape detected.");
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_VEHICLE_BICYCLE, new_alarm, smart_description, NULL);
    }
    else if (ALARM_PD_ID_CAR == class_id || ALARM_PD_ID_BUS == class_id || ALARM_PD_ID_TRUCKS == class_id)     //小汽车 大巴 货车
    {
        snprintf(smart_description, sizeof(smart_description), "video Vehicle shape detected.");
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_VEHICLE_CAR, new_alarm, smart_description, NULL);
    }
    else if(ALARM_PD_ID_MOTOR == class_id)      //摩托
    {
        snprintf(smart_description, sizeof(smart_description), "video Motorcycle shape detected.");
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_VEHICLE_MOTO, new_alarm, smart_description, NULL);
    }
    else
    {
        __ERR("error alarm class id:%d\n", class_id);
        iRet = -1;
    }

    return iRet;
}


static int anj_alarm_ai_detect_update_time(int camera, int level, unsigned long long now_time, int *pnew_alarm)
{
    int link_flag = 0;

    anj_mutex_lock(&s_AlarmMutex);

    if (camera < ANJ_CAMERA_MAX_NUMS)
    {
        if (now_time - s_stAnjAlarmInfo.stAiEvent[camera][level].last_link_time >= ALARM_LINK_TIMEVAL)
        {
            link_flag = 1;
            s_stAnjAlarmInfo.stAiEvent[camera][level].last_link_time = now_time;
        }

        s_stAnjAlarmInfo.stAiEvent[camera][level].trig_time = now_time;
        if (s_stAnjAlarmInfo.stAiEvent[camera][level].event_status == ALARM_EVENT_END)
        {
            *pnew_alarm = 1;
        }
        
        s_stAnjAlarmInfo.stAiEvent[camera][level].event_status = ALARM_EVENT_BEGIN;
    }
    else
    {
        __ERR("alarm camera_index:%d > max camera_index:%d!\n", camera, ANJ_CAMERA_MAX_NUMS);
    }

    anj_mutex_unlock(&s_AlarmMutex);

    return link_flag;
}

static int anj_alarm_motion_detect_update_time(int camera, unsigned long long now_time, int *pnew_alarm)
{
    int link_flag = 0;

    anj_mutex_lock(&s_AlarmMutex);
    if (camera < ANJ_CAMERA_MAX_NUMS)
    {
        if (now_time - s_stAnjAlarmInfo.stMotion[camera].last_link_time >= ALARM_LINK_TIMEVAL)
        {
            link_flag = 1;
            s_stAnjAlarmInfo.stMotion[camera].last_link_time = now_time;
        }

        s_stAnjAlarmInfo.stMotion[camera].trig_time = now_time;
        if (s_stAnjAlarmInfo.stMotion[camera].event_status == ALARM_EVENT_END)
        {
            *pnew_alarm = 1;
        }
        s_stAnjAlarmInfo.stMotion[camera].event_status = ALARM_EVENT_BEGIN;
    }
    else
    {
        __ERR("alarm camera_index:%d > max camera_index:%d!\n", camera, ANJ_CAMERA_MAX_NUMS);
    }

    anj_mutex_unlock(&s_AlarmMutex);

    return link_flag;
}


static int anj_alarm_video_cover_update_time(unsigned long long now_time)
{
    int link_flag = 0;
    static unsigned long long sLastTriggerTime = 0;

    if (now_time >= sLastTriggerTime + ALARM_LINK_TIMEVAL)
    {
        sLastTriggerTime = now_time;
        link_flag = 1;
    }

    return link_flag;
}

static int anj_alarm_audio_cry_update_time(unsigned long long now_time,  int *new_alarm)
{
    int link_flag = 0;

    anj_mutex_lock(&s_AlarmMutex);
    if (now_time - s_stAnjAlarmInfo.stAudioCry.last_link_time >= ALARM_LINK_TIMEVAL)
    {
        link_flag = 1;
        s_stAnjAlarmInfo.stAudioCry.last_link_time = now_time;
    }
    if (s_stAnjAlarmInfo.stAudioCry.event_status == ALARM_EVENT_END)
    {
        *new_alarm = 1;
    }

    s_stAnjAlarmInfo.stAudioCry.trig_time = now_time;
    s_stAnjAlarmInfo.stAudioCry.event_status = ALARM_EVENT_BEGIN;
    anj_mutex_unlock(&s_AlarmMutex);

    return link_flag;
}

static int anj_alarm_audio_lsa_update_time(unsigned long long now_time, int *new_alarm)
{
    int link_flag = 0;

    anj_mutex_lock(&s_AlarmMutex);
    if (now_time - s_stAnjAlarmInfo.stAudioLsa.last_link_time >= ALARM_LINK_TIMEVAL)
    {
        link_flag = 1;
        s_stAnjAlarmInfo.stAudioLsa.last_link_time = now_time;
    }
    if (s_stAnjAlarmInfo.stAudioLsa.event_status == ALARM_EVENT_END)
    {
        *new_alarm = 1;
    }

    s_stAnjAlarmInfo.stAudioLsa.trig_time = now_time;
    s_stAnjAlarmInfo.stAudioLsa.event_status = ALARM_EVENT_BEGIN;
    anj_mutex_unlock(&s_AlarmMutex);

    return link_flag;
}

static int anj_alarm_sd0_spacelow_update_time(unsigned long long now_time)
{
    int link_flag = 0;
    static unsigned long long sLastTriggerTime = 0;

    if (now_time >= sLastTriggerTime + ALARM_SD_SPACELOW_TIMEVAL)
    {
        sLastTriggerTime = now_time;
        link_flag = 1;
    }

    return link_flag;
}

static int anj_alarm_ai_detect_stop(int camera, int level)
{
    int iRet = 0;

    if (level == ALARM_AI_PD)
    {
        char *ai_description = "video Human shape detected disappear.";
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI_FINISH, ALARM_FLAG_OCCUR, ALARM_AI_PD, 0, ai_description, NULL);
    }
    else if (level == ALARM_AI_VEHICLE_CAR)
    {
        char *ai_description = "video Human shape detected disappear.";
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI_FINISH, ALARM_FLAG_OCCUR, ALARM_AI_VEHICLE_CAR, 0, ai_description, NULL);
    }

    return iRet;
}

static int anj_alarm_audio_cry_stop()
{
    int iRet = 0;

    char *cry_description = "baby cry alarm disappeared";
    iRet = anj_alarm_event_handle(0, ALARM_CODE_AUDIO_BABYCRY, ALARM_FLAG_DISAPPEAR, ALARM_LEVEL_EVENT, 0, cry_description, NULL);
    return iRet;
}

static int anj_alarm_audio_lsa_stop()
{
    int iRet = 0;

    char *lsa_description = "loud sound alarm disappeared";
    iRet = anj_alarm_event_handle(0, ALARM_CODE_AUDIO_LSA, ALARM_FLAG_DISAPPEAR, ALARM_LEVEL_EVENT, 0, lsa_description, NULL);
    return iRet;
}

static int anj_alarm_motion_detect_stop(int camera)
{
    int iRet = 0;
    AlarmConfig *palarmcfg = (AlarmConfig *)getAlarmConfig();

    if (0 == palarmcfg->normalAlarm.motionDetectAlarm[0].enable)
    {
        char *md_description = "MD disabled";
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_MOTION_DETECT_DISAPPEAR, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 0, md_description, NULL);
    }
    else
    {
        char *md_description = "motion detect disappear";
        iRet = anj_alarm_event_handle(camera, ALARM_CODE_MOTION_DETECT_DISAPPEAR, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 0, md_description, NULL);
    }

    return iRet;
}

int anj_alarm_ai_detect_start(int camera, int class_id)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int CarEnable = 0;
    int MotorCycleEnable = 0;
    int BicycleEnable = 0;
    int HdEnable = 0;

    AlarmConfig *pAlarmcfg = (AlarmConfig *)getAlarmConfig();

    // step1. 判断配置 需要判断pd + vg
    PdAlarm *pdalarmcfg = &pAlarmcfg->aiAlarm.pdAlarm[camera];
    VideoGateAlarm *pvgalarmcfg = &pAlarmcfg->aiAlarm.vgAlarm[camera];

    int pd_enable = pdalarmcfg->enable;
    
    int iIndex = 0;
    int vg_enable = 0;
    if (0 == pvgalarmcfg->enable)
    {
        vg_enable = 0;
    }
    else
    {
        for (iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
        {
            if (pvgalarmcfg->data[iIndex].enable)
            {
                vg_enable = 1;
                break;
            }
        }
    }

    if (pd_enable == 0 && vg_enable == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    int is_night = alarm_is_night_check();
    int bInPdArmingTime = alarm_arming_with_timespan_check(pdalarmcfg->arming_flag, is_night, &pdalarmcfg->timeSpan);
    int bInVgArmingTime = alarm_arming_with_timespan_check(pvgalarmcfg->arming_flag, is_night, &pvgalarmcfg->timeSpan);
    if (bInVgArmingTime == 0 && bInPdArmingTime == 0)
    {
        iRet = -1;
        __ERR("pd & vg alarm filter enable and arming time\n");
        goto endFunc;
    }

    anj_alarm_parse_detect_type_enable(pdalarmcfg->type, &CarEnable, &MotorCycleEnable, &BicycleEnable, &HdEnable);
    if (1 == vg_enable)
    {
		int temp[4] = {0};
		for(iIndex = 0; iIndex < MAX_VIDEO_VG_LINE; iIndex++)
		{
			if( pvgalarmcfg->data[iIndex].enable )
			{
				anj_alarm_parse_detect_type_enable(pvgalarmcfg->data[iIndex].type, &temp[0], &temp[1], &temp[2], &temp[3]);
				CarEnable = temp[0] ? 1 : CarEnable;
				MotorCycleEnable = temp[1] ? 1 : MotorCycleEnable;
				BicycleEnable = temp[2] ? 1 : BicycleEnable;
				HdEnable = temp[3] ? 1 : HdEnable;
			}
		}
    }

    int result = 0;
    int ai_event_index = 0;             // classid需要转换一次index记录触发时间

    int ai_human_car_mix = 0;
    if (ALARM_PD_ID_HUMAN == class_id && 1 == HdEnable)                 //人形
    {
        result = 1;
        ai_event_index = ALARM_AI_PD;
    }
    else if (ALARM_PD_ID_BICYCLE == class_id && 1 == BicycleEnable)     // 自行车
    {
        result = 1;
        ai_event_index = ALARM_AI_VEHICLE_BICYCLE;
    }
    else if ((ALARM_PD_ID_CAR == class_id || ALARM_PD_ID_BUS == class_id || ALARM_PD_ID_TRUCKS == class_id) && 1 == CarEnable)   //汽车
    {
        result = 1;
        ai_event_index = ALARM_AI_VEHICLE_CAR;
    }
    else if (ALARM_PD_ID_MOTOR == class_id && 1 == MotorCycleEnable)     //摩托车
    {
        result = 1;
        ai_event_index = ALARM_AI_VEHICLE_MOTO;
    }
    else if (ALARM_PD_ID_HUMAN_CAR_FIXED == class_id && 1 == HdEnable && 1 == CarEnable)   // 人车混合
    {
        result = 1;
        ai_human_car_mix = 1;
    }
    else
    {
        result = 0;
    }

    // 自行车/摩托车按人形上报
    if (class_id == ALARM_PD_ID_BICYCLE || class_id == ALARM_PD_ID_MOTOR)
    {
        class_id = ALARM_PD_ID_HUMAN;
    }

    //__INFO("alarm class_id:%d, ai_event_idx:%d, human_car_mix:%d\n", class_id, ai_event_index, ai_human_car_mix);

    if (0 == result)
    {
        iRet = -1;
        goto endFunc;
    }

    // step2. 更新时间
    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);
    int new_alarm = 0;

    if (ai_human_car_mix == 0)
    {
        iRet = anj_alarm_ai_detect_update_time(camera, ai_event_index, now_time, &new_alarm);
        if (iRet == 0)
        {
            iRet = -2;
            goto endFunc;
        }

        iRet = anj_alarm_ai_detect_trigger(camera, class_id, new_alarm);
    }
    else    // 人车混合需要发送两条报警事件而不是一个人车混合
    {
        static int ai_huam_car_order = 0;   // 人形车型上报顺序（先人后车 再先车后人）
        if (ai_huam_car_order == 0)
        {
            ai_huam_car_order = 1;
            iRet = anj_alarm_ai_detect_update_time(camera, ALARM_AI_PD, now_time, &new_alarm);
            if (iRet)
            {
                iRet = anj_alarm_ai_detect_trigger(camera, ALARM_PD_ID_HUMAN, new_alarm);
            }

            iRet = anj_alarm_ai_detect_update_time(camera, ALARM_AI_VEHICLE_CAR, now_time, &new_alarm);
            if (iRet)
            {
                iRet = anj_alarm_ai_detect_trigger(camera, ALARM_PD_ID_CAR, new_alarm);
            }       
        }
        else
        {
            ai_huam_car_order = 0;
            iRet = anj_alarm_ai_detect_update_time(camera, ALARM_AI_VEHICLE_CAR, now_time, &new_alarm);
            if (iRet)
            {
                iRet = anj_alarm_ai_detect_trigger(camera, ALARM_PD_ID_CAR, new_alarm);
            }

            iRet = anj_alarm_ai_detect_update_time(camera, ALARM_AI_PD, now_time, &new_alarm);
            if (iRet)
            {
                iRet = anj_alarm_ai_detect_trigger(camera, ALARM_PD_ID_HUMAN, new_alarm);
            }
        }
    }

    return iRet;

endFunc:
    //__INFO("alarm ai detect failed:%d!\n", iRet);
    return iRet;
}

int anj_alarm_audio_lsa_start()
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int enable = 0;
    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);

    AlarmConfig *palarmcfg = (AlarmConfig *)getAlarmConfig();
    // step1.
    if (0 == palarmcfg->aiAlarm.audioAlarm.enable_lsd)
    {
        return -1;
    }

    // step2.
    int is_new_alarm = 0;
    enable = anj_alarm_audio_lsa_update_time(now_time, &is_new_alarm);
    if (enable == 0)
    {
        return -2;
    }

    // step3.
    char *lsa_description = "loud sound alarm occurred";
    iRet = anj_alarm_event_handle(0, ALARM_CODE_AUDIO_LSA, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, is_new_alarm, lsa_description, NULL);
endFunc:
    return iRet;
}

int anj_alarm_audio_cry_start()
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int enable = 0;
    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);

    AlarmConfig *palarmcfg = (AlarmConfig *)getAlarmConfig();

    // step1.
    if (0 == palarmcfg->aiAlarm.audioAlarm.enable_babycry)
    {
        return -1;
    }

    // step2.
    int is_new_alarm = 0;
    enable = anj_alarm_audio_cry_update_time(now_time, &is_new_alarm);
    if (enable == 0)
    {
        return -2;
    }

    // step3.
    char *cry_description = "baby cry alarm occurred";
    iRet = anj_alarm_event_handle(0, ALARM_CODE_AUDIO_BABYCRY, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, is_new_alarm, cry_description, NULL);
endFunc:
    return iRet;
}

int anj_alarm_sd0_spacelow_start()
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int enable = 0;
    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);
    AlarmConfig *pAlarmcfg = (AlarmConfig *)getAlarmConfig();

    // step1
    if (0 == pAlarmcfg->normalAlarm.storageFullAlarm.enable)
    {
        return -1;
    }

    // step2
    enable = anj_alarm_sd0_spacelow_update_time(now_time);
    if (enable == 0)
    {
        return -2;
    }

    // step3
    char *sd_description = "sd free space too small";
    iRet = anj_alarm_event_handle(0, ALARM_CODE_SD0_FREESPACE_LOW, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 1, sd_description, NULL);
endFunc:
    return iRet;
}

int anj_alarm_gpio3_high2low_start(int chn)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");

    if (chn < 1 || chn > 4)
    {
        iRet = -1;
        goto endFunc;
    }

    AlarmConfig *pAlarmcfg = (AlarmConfig *)getAlarmConfig();
    AlarmChannel *pChnCfg = &pAlarmcfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];

    if (pChnCfg->enable == 0)
    {
        iRet = -2;
        goto endFunc;
    }

    /* 常开(LOW-HIGH)：开路=结束，不受 timespan 限制；FINISH 由 link_process 转换 */
    if (strcasecmp(pChnCfg->triggerType.name, "LOW-HIGH") == 0) // 常开
    {
        char alarmdata[128];
        sprintf(alarmdata, "IO %d INPUT OPEN", chn);
        return anj_alarm_event_handle(chn, ALARM_CODE_IO_ALARM, ALARM_FLAG_OCCUR,
                                      ALARM_LEVEL_EVENT, 1, alarmdata, NULL);
    }

    if (strcasecmp(pChnCfg->triggerType.name, "HIGH-LOW") != 0) // 常闭
    {
        iRet = 0;
        goto endFunc;
    }

    if (0 == CheckNowIsInTimeSpan(&pChnCfg->timeSpan))
    {
        iRet = -2;
        goto endFunc;
    }

    iRet = anj_alarm_event_handle(chn, ALARM_CODE_GPIO3_HIGH2LOW, ALARM_FLAG_OCCUR,
                                  ALARM_LEVEL_EVENT, 1, "IOINPUT ALARM", NULL);
    return iRet;

endFunc:
    return iRet;
}

int anj_alarm_gpio3_low2high_start(int chn)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");

    if (chn < 1 || chn > 4)
    {
        iRet = -1;
        goto endFunc;
    }

    AlarmConfig *pAlarmcfg = (AlarmConfig *)getAlarmConfig();
    AlarmChannel *pChnCfg = &pAlarmcfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];

    if (pChnCfg->enable == 0)
    {
        iRet = -2;
        goto endFunc;
    }

    /* 常闭(HIGH-LOW)：闭合=结束，不受 timespan 限制 */
    if (strcasecmp(pChnCfg->triggerType.name, "HIGH-LOW") == 0)
    {
        return anj_alarm_event_handle(chn, ALARM_CODE_GPIO3_LOW2HIGH, ALARM_FLAG_OCCUR,
                                      ALARM_LEVEL_EVENT, 1, "IOINPUT ALARM", NULL);
    }

    if (strcasecmp(pChnCfg->triggerType.name, "LOW-HIGH") != 0)
    {
        iRet = 0;
        goto endFunc;
    }

    if (0 == CheckNowIsInTimeSpan(&pChnCfg->timeSpan))
    {
        iRet = -2;
        goto endFunc;
    }

    iRet = anj_alarm_event_handle(chn, ALARM_CODE_GPIO3_LOW2HIGH, ALARM_FLAG_OCCUR,
                                  ALARM_LEVEL_EVENT, 1, "IOINPUT ALARM", NULL);
    return iRet;

endFunc:
    return iRet;
}

int anj_alarm_video_cover_start(int camera)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int enable = 0;
    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);

    AlarmConfig *palarmcfg = (AlarmConfig *)getAlarmConfig();
    VideoCoverAlarm *pVideoCoverAlarm = &palarmcfg->normalAlarm.videoCoverAlarm[camera];

    // step1
    if (pVideoCoverAlarm->enable == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    enable = CheckNowIsInTimeSpan(&pVideoCoverAlarm->timeSpan);
    if (enable == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    // step2.
    enable = anj_alarm_video_cover_update_time(now_time);
    if (enable == 0)
    {
        iRet = -2;
        goto endFunc;
    }

    // step3.
    char *vc_description = "cover alarm";
    iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_COVERD, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 1, vc_description, NULL);
    return iRet;

endFunc:
    __ERR("video cover start failed ret:%d\n", iRet);
    return iRet;
}

int anj_alarm_video_lost_start()
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    // todo 暂无实现
endFunc:
    return iRet;
}

int anj_alarm_video_gate_start(int camera)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    AlarmConfig *palarmcfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pvgalarmcfg = &palarmcfg->aiAlarm.vgAlarm[camera];

    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);

    // step1. 检测配置
    if (pvgalarmcfg->enable == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    int night = alarm_is_night_check();
    iRet = alarm_arming_with_timespan_check(pvgalarmcfg->arming_flag, night, &pvgalarmcfg->timeSpan);
    if(0 == iRet)
    {
        iRet = -1;
        goto endFunc;
    }

    // step2. 更新时间
    int is_new_alarm = 0;
    iRet = anj_alarm_ai_detect_update_time(camera, ALARM_AI_VIDEO_GATE, now_time, &is_new_alarm);
    if (0 == iRet)
    {
        iRet = -2;
        goto endFunc;
    }

    char *vg_description = "tripwire video gate detected.";
    iRet = anj_alarm_event_handle(camera, ALARM_CODE_VIDEO_AI, ALARM_FLAG_OCCUR, ALARM_AI_VIDEO_GATE, is_new_alarm, vg_description, NULL);
    return iRet;

endFunc:
    __ERR("alarm video gate failed:%d\n", iRet);
    return iRet;
}

int anj_alarm_motion_detect_start(int camera)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int hd_allow_md_flag = 0;

    AlarmConfig *pAlarmcfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pMdAlarm = &pAlarmcfg->normalAlarm.motionDetectAlarm[camera];
    PdAlarm *pPdAlarm = &pAlarmcfg->aiAlarm.pdAlarm[camera];

    unsigned long long now_time = anj_mw_get_cputime_ms(NULL);

    // step1. 检测配置
    if (0 == pMdAlarm->enable)
    {
        iRet = -1;
        goto endFunc;
    }

    int is_night = alarm_is_night_check();
    iRet = alarm_arming_with_timespan_check(pMdAlarm->arming_flag, is_night, &(pMdAlarm->timeSpan));
    if (iRet == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    if(!anj_alarm_motion_block_valid_check(pMdAlarm))
    {
        iRet = -1;
        goto endFunc;
    }

    if (0 == pPdAlarm->allowMd)         // 检测人形是否允许移动侦测
    {
        if (0 == pPdAlarm->enable)
        {
            hd_allow_md_flag = 1;
        }
        else if(0 == alarm_arming_with_timespan_check(pPdAlarm->arming_flag, is_night, &pPdAlarm->timeSpan))
        {
            hd_allow_md_flag = 1;
        }
    }
    else
    {
        hd_allow_md_flag = 1;
    }

    if (hd_allow_md_flag == 0)
    {
        iRet = -1;
        goto endFunc;
    }

    // step2. 更新时间 判断是否需要联动
    int new_alarm = 0;
    iRet = anj_alarm_motion_detect_update_time(camera, now_time, &new_alarm);
    if (iRet == 0)
    {
        iRet = -2;
        goto endFunc;
    }

    // step3. 开始联动
    char *md_description = "motion alarm.";
    iRet = anj_alarm_event_handle(camera, ALARM_CODE_MOTION_DETECT, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, new_alarm, md_description, NULL);

    return iRet;
endFunc:
    //__ERR("motion alarm start failed:%d!\n", iRet);
    return iRet;
}

int anj_alarm_ioinput_manual_trigger()
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    int alarm_chn = 1;
    char alarm_data[32] = {0};
    snprintf(alarm_data, sizeof(alarm_data), "IO %d INPUT OPEN", alarm_chn);

    iRet = anj_alarm_event_handle(alarm_chn, ALARM_CODE_IO_ALARM, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 1, alarm_data, NULL);
endFunc:
    return iRet;
}

int anj_alarm_sdcard_format_start(int bSuccess)
{
    int iRet = 0;

    ANJ_CHK((0 != s_stAlarmInit), iRet, "not init");
    if (bSuccess)
    {
        iRet = anj_alarm_event_handle(0, ALARM_CODE_SD_FORMAT_FINISH, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 1, "sd format finish", NULL);
    }
    else
    {
        iRet = anj_alarm_event_handle(0, ALARM_CODE_SD_FORMAT_FAIL, ALARM_FLAG_OCCUR, ALARM_LEVEL_EVENT, 1, "sd format fail", NULL);
    }

endFunc:
    return iRet;
}

static int anj_alarm_timer_thread(void *ctx, int *bStart)
{
    unsigned long long tNowTime = 0;
    int end_flag = 0;

    int index = 0;
    int camera_index = 0;

    while(bStart && *bStart)
    {
        end_flag = 0;
        tNowTime = anj_mw_get_cputime_ms(NULL);

        // check ai
        anj_mutex_lock(&s_AlarmMutex);
        for (camera_index = 0; camera_index < ANJ_CAMERA_MAX_NUMS; camera_index++)
        {
            for (index = 0; index < ALARM_AI_MAX; index++)
            {
                if (s_stAnjAlarmInfo.stAiEvent[camera_index][index].event_status != ALARM_EVENT_END)
                {
                    end_flag = anj_alarm_check_disappear(s_stAnjAlarmInfo.stAiEvent[camera_index][index].trig_time, tNowTime, ALARM_DISAPPEAR_TIMEVAL);
                    if (1 == end_flag)
                    {
                        __INFO("ai alarm disappear camera:%d level:%d\n", camera_index, index);
                        s_stAnjAlarmInfo.stAiEvent[camera_index][index].event_status = ALARM_EVENT_END;
                        anj_alarm_ai_detect_stop(camera_index, index);
                    }
                }
            }

            // check md
            if (s_stAnjAlarmInfo.stMotion[camera_index].event_status != ALARM_EVENT_END)
            {
                end_flag = anj_alarm_check_disappear(s_stAnjAlarmInfo.stMotion[camera_index].trig_time, tNowTime, ALARM_DISAPPEAR_TIMEVAL);
                if (1 == end_flag)
                {
                    __INFO("alarm md disappear!\n");
                    s_stAnjAlarmInfo.stMotion[camera_index].event_status = ALARM_EVENT_END;
                    anj_alarm_motion_detect_stop(camera_index);
                }
            }
        }

        // check audio lsa & baby cry
        if (s_stAnjAlarmInfo.stAudioLsa.event_status != ALARM_EVENT_END)
        {
            end_flag = anj_alarm_check_disappear(s_stAnjAlarmInfo.stAudioLsa.trig_time, tNowTime, ALARM_DISAPPEAR_TIMEVAL);
            if (1 == end_flag)
            {
                __INFO("alarm audio lsa disappear!\n");
                s_stAnjAlarmInfo.stAudioLsa.event_status = ALARM_EVENT_END;
                anj_alarm_audio_lsa_stop();
            }
        }

        if (s_stAnjAlarmInfo.stAudioLsa.event_status != ALARM_EVENT_END)
        {
            end_flag = anj_alarm_check_disappear(s_stAnjAlarmInfo.stAudioLsa.trig_time, tNowTime, ALARM_DISAPPEAR_TIMEVAL);
            if (1 == end_flag)
            {
                __INFO("alarm audio baby cry disappear!\n");
                s_stAnjAlarmInfo.stAudioLsa.event_status = ALARM_EVENT_END;
                anj_alarm_audio_cry_stop();
            }
        }

        anj_mutex_unlock(&s_AlarmMutex);

        /* IO 输入持续告警：活跃态每 ALARM_LINK_TIMEVAL 重发 IO_ALARM（对齐老 check_io_alarm） */
        if (anj_mw_hwctrl_alarmin_port_count_get() > 0)
        {
            static unsigned long long s_stAlarmIoLastSend[MAX_ALARMCHANNEL_COUNT] = {0};
            int chn = 0;
            AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();

            for (chn = 1; chn <= MAX_ALARMCHANNEL_COUNT; chn++)
            {
                AlarmChannel *pChnCfg = &pAlarmCfg->normalAlarm.inputAlarm.alarmChannels[chn - 1];
                int state = 0;
                int active = 0;

                state = anj_mw_hwctrl_alarmin_chn_status_get(chn);
                if (state < 0)
                {
                    s_stAlarmIoLastSend[chn - 1] = 0;
                    continue;
                }

                if (pChnCfg->enable == 0 || 0 == CheckNowIsInTimeSpan(&pChnCfg->timeSpan))
                {
                    s_stAlarmIoLastSend[chn - 1] = 0;
                    continue;
                }

                if (state == 0 && strcasecmp(pChnCfg->triggerType.name, "HIGH-LOW") == 0) //常闭
                {
                    active = 1;
                }
                else if (state == 1 && strcasecmp(pChnCfg->triggerType.name, "LOW-HIGH") == 0) //常开
                {
                    active = 1;
                }

                if (!active)
                {
                    s_stAlarmIoLastSend[chn - 1] = 0;
                    continue;
                }

                if (tNowTime - s_stAlarmIoLastSend[chn - 1] >= ALARM_LINK_TIMEVAL)
                {
                    char alarm_data[64] = {0};
                    if (state == 0)
                    {
                        snprintf(alarm_data, sizeof(alarm_data), "IO %d INPUT OPEN", chn);
                    }
                    else
                    {
                        snprintf(alarm_data, sizeof(alarm_data), "IO %d INPUT CLOSE", chn);
                    }
                    anj_alarm_event_handle(chn, ALARM_CODE_IO_ALARM, ALARM_FLAG_OCCUR,
                                           ALARM_LEVEL_EVENT, 1, alarm_data, NULL);
                    s_stAlarmIoLastSend[chn - 1] = tNowTime;
                }
            }
        }

        usleep(200 * 1000);
    }

    return 0;
}

static void anj_alarm_capability_init()
{
    int count = 0;
    if (ALARM_SUPPORT_SMART_AI_FACE_REC)
    {
        anj_sysctl_capability_add(FUNCTION_FACE_FR);
    }
    if (ALARM_SUPPORT_SMART_AI_FIRE)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_FIRE);
    }
    if (ALARM_SUPPORT_SMART_AI_LPR)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_LPR);
    }
    if (ALARM_SUPPORT_MOTION)
    {
        anj_sysctl_capability_add(FUNCTION_MD_18X22);
    }
    if (ALARM_SUPPORT_VIDEO_GATE)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_VIDEOGATE_BY_PD);
    }
    if (ALARM_SUPPORT_REGION_AI)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_REGION_AI);
    }
    if (ALARM_SUPPORT_VIDEO_COVERD)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_COVER);
    }

    if (ALARM_SUPPORT_IO_IN)
    {
        anj_sysctl_capability_add(FUNCTION_GPIO_INPUT);
        count = anj_mw_hwctrl_alarmin_port_count_get();
        if (count == 1)
        {
            anj_sysctl_capability_add(FUCTION_ONE_INPUT);
        }
        else if (count == 2)
        {
            anj_sysctl_capability_add(FUCTION_TWO_INPUT);
        }
        else if (count == 3)
        {
            anj_sysctl_capability_add(FUCTION_THREE_INPUT);
        }
        else if (count == 4)
        {
            anj_sysctl_capability_add(FUCTION_FOUR_INPUT);
        }
    }

    if (ALARM_SUPPORT_IO_OUT)
    {
        anj_sysctl_capability_add(FUNCTION_GPIO_OUTPUT);
        anj_sysctl_capability_add(FUCTION_IO_OUTPUT_SET);
        anj_sysctl_capability_add(FUNCTION_IOOUT_ARMING);

        count = anj_mw_hwctrl_alarmout_port_count_get();
        if (count == 1)
        {
            anj_sysctl_capability_add(FUCTION_ONE_OUTPUT);
        }
        else if (count == 2)
        {
            anj_sysctl_capability_add(FUCTION_TWO_OUTPUT);
        }
        else if (count == 3)
        {
            anj_sysctl_capability_add(FUCTION_THREE_OUTPUT);
        }
        else if (count == 4)
        {
            anj_sysctl_capability_add(FUCTION_FOUR_OUTPUT);
        }
    }

    if (ANJ_GPIO_PORT_ALARM_LED > 0 || ANJ_EXPAND_DEV_GPIO > 0)
    {
        anj_sysctl_capability_add(FUNCTION_ALARM_LED); // 红蓝警灯
    }

}

static int anj_alarm_ioin_monitor_thread(void *ctx, int *bStart)
{
    int chn = 0;
    int cur = 0;
    int prev = 0;

    (void)ctx;
    while (bStart && *bStart)
    {
        for (chn = 1; chn <= MAX_ALARMCHANNEL_COUNT; chn++)
        {
            cur = anj_mw_hwctrl_alarmin_chn_status_get(chn);
            if (cur < 0)
            {
                continue;
            }

            prev = s_alarmin_last_state[chn];
            if (prev != cur)
            {
                s_alarmin_last_state[chn] = cur;
                if (prev == 1 && cur == 0)
                {
                    anj_alarm_gpio3_high2low_start(chn);
                }
                else if (prev == 0 && cur == 1)
                {
                    anj_alarm_gpio3_low2high_start(chn);
                }
            }
        }
        usleep(100 * 1000);
    }
    return 0;
}

static int anj_alarm_ioin_monitor_start(void)
{
    int chn = 0;
    int state = 0;

    if (anj_mw_hwctrl_alarmin_port_count_get() <= 0)
    {
        return 0;
    }

    if (s_stAlarminMonitorThread.start > 0)
    {
        __ERR("alarmin monitor already start\n");
        return 0;
    }

    memset(s_alarmin_last_state, 0, sizeof(s_alarmin_last_state));
    for (chn = 1; chn <= MAX_ALARMCHANNEL_COUNT; chn++)
    {
        state = anj_mw_hwctrl_alarmin_chn_status_get(chn);
        if (state >= 0)
        {
            s_alarmin_last_state[chn] = state; /* seed，不触发边沿 */
        }
    }

    memset(&s_stAlarminMonitorThread, 0, sizeof(anj_thread_s));
    s_stAlarminMonitorThread.bAutoDestroy = 1;
    snprintf(s_stAlarminMonitorThread.iThreadName, sizeof(s_stAlarminMonitorThread.iThreadName),
             "alarmin_monitor");
    s_stAlarminMonitorThread.iThreadjob.ctx = &s_stAlarminMonitorThread;
    s_stAlarminMonitorThread.iThreadjob.func = anj_alarm_ioin_monitor_thread;
    return anj_thread_task_create(&s_stAlarminMonitorThread);
}

static void anj_alarm_ioin_monitor_stop(void)
{
    if (s_stAlarminMonitorThread.start > 0)
    {
        anj_thread_task_destroy(&s_stAlarminMonitorThread, 0);
    }
}

int anj_alarm_ioout_channel_apply(int portIndex, const char *triggerType)
{
    if (portIndex < 1 || portIndex > 4)
    {
        return 0;
    }

    if (triggerType == NULL || triggerType[0] == '\0')
    {
        return 0;
    }

    if (anj_mw_hwctrl_alarmout_get_port(portIndex) <= 0)
    {
        return 0;
    }

    return anj_mw_hwctrl_alarmout_init(portIndex, triggerType);
}

static void anj_alarm_ioout_init(void)
{
    int i = 0;
    AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
    OutPutAlarm *pOutputAlarm = &pAlarmCfg->normalAlarm.outputAlarm;

    for (i = 0; i < MAX_OUTPUT_CHANENL_COUNT; i++)
    {
        OutputChannel *pChn = &pOutputAlarm->outputChannels[i];
        anj_alarm_ioout_channel_apply(pChn->portIndex, pChn->triggerType.name);
    }
}

int anj_alarm_init(void)
{
    int iRet = 0;

    ANJ_CHK((0 == s_stAlarmInit), iRet, "had been init");
    anj_alarm_capability_init();

    memset(&s_stAnjAlarmInfo, 0, sizeof(AnjAlarmInfo_t));

    memset(&s_stAlarmTimerThread, 0, sizeof(anj_thread_s));
    s_stAlarmTimerThread.bAutoDestroy = 1;
    strncpy(s_stAlarmTimerThread.iThreadName, "anj_alarm_timer_thread", sizeof(s_stAlarmTimerThread.iThreadName) - 1);
    s_stAlarmTimerThread.iThreadjob.ctx = &s_stAlarmTimerThread;
    s_stAlarmTimerThread.iThreadjob.func = anj_alarm_timer_thread;
    iRet = anj_thread_task_create(&s_stAlarmTimerThread);

    anj_mw_hwctrl_alarmin_init();
    anj_alarm_ioout_init();
    s_stAlarmInit = 1;
    anj_alarm_ioin_monitor_start();
endFunc:
    return iRet;
}

int anj_alarm_uninit(void)
{
    int iRet = -1;

    ANJ_CHK((1 == s_stAlarmInit), iRet, "not init");
    anj_alarm_ioin_monitor_stop();
    anj_mw_hwctrl_alarmin_uninit();
    alarm_link_ptz_action_stop();

    iRet = anj_thread_task_destroy(&s_stAlarmTimerThread, 0);
    s_stAlarmInit = 0;
endFunc:
    return iRet;
}


REGISTER_MODULE(anj_alarm, MODULE_PRIORITY_ALARM);

