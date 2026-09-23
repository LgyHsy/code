#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_thread.h"

#include "anj_sysctl.h"
#include "anj_config.h"
#include "function_list.h"
#include "eventhub.h"
#include "cJSON.h"

#include "http_def.h"
#include "http_handle.h"
#include "http_unv.h"
#include "unv_def.h"
#include "unv_get.h"
#include "unv_put.h"


#define UNV_SEARCH_TIME_CNT (300)       // 宇视协议搜索时间 30s 

extern int g_onvif_expand;              // 宇视私有协议入口开关
extern int g_yen_version_smd;           // 宇视协议 新款 (SMD)
extern int g_yen_version_sdm;           // 宇视协议 旧款 (SDM)

static unv_info_t s_stAnjUnvInfo;
static pthread_mutex_t s_UnvinfoMutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_stUnvSearchTimingThread;



static char unv_str_response[] = 
"{\r\n"
"\"Response\": {\r\n"
"\t\"ResponseURL\":\"%s\",\r\n"
"\t\"CreatedID\":-1,\r\n"
"\t\"ResponseCode\":0,\r\n"
"\t\"SubResponseCode\":0,\r\n"
"\t\"ResponseString\":\"Succeed\",\r\n"
"\t\"StatusCode\":0,\r\n"
"\t\"StatusString\":\"Succeed\",\r\n"
"\t\"Data\":%s\r\n"
"\t}\r\n"
"}\r\n";


static int s_motion_detect_mode = 0;        // motion mode:0-md 1-umd
int unv_motion_detect_mode_get()
{
    return s_motion_detect_mode;
}
void unv_motion_detect_mode_set(int mode)
{
    s_motion_detect_mode = mode;
}

int unv_smart_support_get()
{
    return s_stAnjUnvInfo.smart_support;
}

int unv_enable_get()
{
    return s_stAnjUnvInfo.unv_enable;
}

static void unv_search_status_get(EventResult *event_result, void *data)
{
    if (event_result)
    {
        anj_mutex_lock(&s_UnvinfoMutex);
        event_result->ret = (s_stAnjUnvInfo.search_status) ? 1 : 0;
        anj_mutex_unlock(&s_UnvinfoMutex);
    }
}

static int unv_search_timing_thread(void *ctx, int *bstart)
{
    int i = 0;

    anj_mutex_lock(&s_UnvinfoMutex);
    s_stAnjUnvInfo.search_status = 1;
    anj_mutex_unlock(&s_UnvinfoMutex);
    
    while(bstart && *bstart)
    {
        if (i <= UNV_SEARCH_TIME_CNT)
        {
            i++;
            usleep(100 * 1000);
        }
        else
        {
            break;
        }
    }

    anj_mutex_lock(&s_UnvinfoMutex);
    s_stAnjUnvInfo.search_status = 0;
    anj_mutex_unlock(&s_UnvinfoMutex);

    __INFO("unv search timeout!\n");
    return 0;
}

int unv_handle_put_request(void* pInst, const char* http_url, const char *pMsgBody, const char* msgbuf, cb_func_http_response pCbResponse)
{
    if(pInst == NULL)
    {
        return -1;
    }
    
    if( pMsgBody == NULL)
    {
        if(strstr(msgbuf, "DELETE") == NULL)
        {
            return -1;
        }
    }

    char prefixstr[64] = {0};
    char prefixstr1[64] = {0};
    snprintf(prefixstr, sizeof(prefixstr), UNV_LAPI_CHANNELS);
    snprintf(prefixstr1, sizeof(prefixstr1), UNV_LAPI_SYSTEM);
    
    char channelid[32] = {0};

    int iRet = HTTP_PUT_OK;
    int strlength = 0;
    char *str = NULL;
    char *configpath = NULL;

    __INFO("http_url:%s\n", http_url);

    //首先判断前缀/LAPI/V1.0/Channels
    if(strncmp(http_url, prefixstr, strlen(prefixstr)) && 
        strncmp(http_url, prefixstr1, strlen(prefixstr1)))
    {
        return -1;
    }

    //获取通道ID 对于IPC, 通道ID无效
    if((str = strstr(http_url + strlen(prefixstr) + 1, "/")) == NULL &&
        (str = strstr(http_url + strlen(prefixstr1) - 1, "/")) == NULL)
    {
        return -1;
    }

    strlength = str - (http_url + strlen(prefixstr) + 1);
    memcpy(channelid, http_url + strlen(prefixstr) + 1, strlength);

    //匹配url
    configpath = str + 1;
    __INFO("configpath:%s\n", configpath);

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    int has_smart = pstAbility->smart_motion;
    int has_human = pstAbility->human_enable;
    int has_car = pstAbility->car_enable;
    int has_gate = pstAbility->video_gate;
    int has_face = pstAbility->face_detect;
    int has_region = pstAbility->region_enable;
    int has_track = pstAbility->auto_track;

    if(!strcmp(configpath, UNV_ALARM_HUMAN_RULE) && 0 && has_human)
    {
        iRet = unv_alarm_human_rule_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_ALARM_HUMAN_AREAS) && 0 && has_human)
    {
        iRet = unv_alarm_human_areas_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_ALARM_HUMAN_WEEKPLAN) && 0 && has_human)
    {
        iRet = unv_alarm_human_week_plan_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    /////////////////////////车型///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_VEHICLE_DETECT_RULE) && 0 && has_car)
    {
        iRet = unv_smart_vehicle_rule_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_VEHICLE_DETECT_AREAS0) && 0 && has_car)
    {
        iRet = unv_smart_vehicle_areas0_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_VEHICLE_DETECT_AREAS) && 0 && has_car)
    {
        iRet = unv_smart_vehicle_areas_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_VEHICLE_WEEKPLAN) && 0 && has_car)
    {
        iRet = unv_alarm_human_week_plan_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    /////////////////////////Audio///////////////////////////////////////
    else if(!strcmp(configpath, UNV_MEDIA_AUDIO_INPUT))
    {
        iRet = unv_media_audio_input_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_MEDIA_AUDIO_OUTPUT))
    {
        iRet = unv_media_audio_output_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    /////////////////////////图像///////////////////////////////////////
    else if(!strcmp(configpath, UNV_IMAGE_ENHANCE))
    {
        iRet = unv_image_enhance_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_IMAGE_LAMPCTRL))
    {
        iRet = unv_image_lampctrl_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    /////////////////////////智能///////////////////////////////////////
    //智能侦测规则
    else if(!strcmp(configpath, UNV_ALARM_MOTION_RULE) && has_smart)
    {
        iRet = unv_alarm_motion_rule_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    //智能侦测区域0
    //else if(!strcmp(configpath, ALARM_MOTION_AREAS0))
    //{
    //	ret = Set_SmartMotion_Rule(pInst, http_url, pMsgBody, pCbResponse );
    //}
    //智能侦测区域all
    else if(!strcmp(configpath, UNV_ALARM_MOTION_AREAS) && has_smart)
    {
        iRet = unv_alarm_motion_areas_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    //智能侦测LINK
    else if(!strcmp(configpath, UNV_ALARM_MOTION_LINK) && has_smart)
    {
        iRet = unv_alarm_motion_linkage_action_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    //智能侦测week
    else if(!strcmp(configpath, UNV_ALARM_MOTION_WEEKPLAN) && has_smart)
    {
        if (unv_motion_detect_mode_get() == 0)
            iRet = unv_alarm_motion_week_plan_set(pInst, http_url, pMsgBody, pCbResponse );
        else
            iRet = unv_alarm_human_week_plan_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    /////////////////////////拌线///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_RULE) && has_gate)
    {
        //Check_yen_version_state(2);
        iRet = unv_smart_crossline_rule_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_LINK) && has_gate)//******************************
    {
        //Check_yen_version_state(2);
        //if (is_Y_Version || MsgGetFunctionEnabled(FUNCTION_LIGHT_ACTION) == 1)

        if (anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION) == 1)
            iRet = unv_smart_crossline_linkage_action_set(pInst, http_url, pMsgBody, pCbResponse );
        else
            iRet = unv_smart_crossline_linkage_action_bright_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_AREAS0) && has_gate)
    {
        iRet = unv_smart_crossline_areas0_set(pInst, http_url, pMsgBody, pCbResponse, 0);
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_AREAS1) && has_gate)
    {
        iRet = unv_smart_crossline_areas0_set(pInst, http_url, pMsgBody, pCbResponse, 1);
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_AREAS2) && has_gate)
    {
        iRet = unv_smart_crossline_areas0_set(pInst, http_url, pMsgBody, pCbResponse, 2);
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_AREAS3) && has_gate)
    {
        iRet = unv_smart_crossline_areas0_set(pInst, http_url, pMsgBody, pCbResponse, 3);
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_AREAS) && has_gate)
    {
        iRet = unv_smart_crossline_areas_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_CROSSLINE_WEEKPLAN) && has_gate)
    {
        iRet = unv_smart_crossline_week_plan_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    /////////////////////////人脸///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_FACE_ENABLE) && has_face)
    {
        iRet = unv_smart_facedetect_enable_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_FACE_RULE) && has_face)
    {
        iRet = unv_smart_facedetect_rule_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_FACE_AREAS) && has_face)
    {
        iRet = unv_smart_facedetect_areas_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_FACE_LINK) && has_face)
    {
        iRet = unv_smart_facedetect_linkage_actions_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_FACE_WEEKPLAN) && has_face)
    {
        iRet = unv_smart_facedetect_week_plan_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    /////////////////////////区域入侵///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_INTRU_RULE) && has_region)
    {
        iRet = unv_smart_intrusion_rule_set(pInst, http_url, pMsgBody, pCbResponse, 0);
    }
    else if(!strcmp(configpath, UNV_SMART_INTRU_AREAS) && has_region)
    {
        iRet = unv_smart_intrusion_areas_set(pInst, http_url, pMsgBody, pCbResponse, 0);
    }
    else if(!strcmp(configpath, UNV_SMART_INTRU_LINK) && has_region)
    {
        iRet = unv_smart_intrusion_linkage_actions_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_SMART_INTRU_WEEKPLAN) && has_region)
    {
        iRet = unv_smart_intrusion_week_plan_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    /////////////////////////进入区域///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_ACCESSZONE_RULE) && has_region)
    {
        iRet = unv_smart_intrusion_rule_set(pInst, http_url, pMsgBody, pCbResponse, 1);
    }
    else if(!strcmp(configpath, UNV_SMART_ACCESSZONE_AREAS) && has_region)
    {
        iRet = unv_smart_intrusion_areas_set(pInst, http_url, pMsgBody, pCbResponse, 1);
    }
    else if(!strcmp(configpath,UNV_SMART_ACCESSZONE_LINK) && has_region)
    {
        iRet = unv_smart_intrusion_linkage_actions_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_SMART_ACCESSZONE_WEEKPLAN) && has_region)
    {
        iRet = unv_smart_intrusion_week_plan_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    /////////////////////////离开区域///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_LEAVEZONE_RULE) && has_region)
    {
        iRet = unv_smart_intrusion_rule_set(pInst, http_url, pMsgBody, pCbResponse, 2);
    }
    else if(!strcmp(configpath, UNV_SMART_LEAVEZONE_AREAS) && has_region)
    {
        iRet = unv_smart_intrusion_areas_set(pInst, http_url, pMsgBody, pCbResponse, 2);
    }
    else if(!strcmp(configpath, UNV_SMART_LEAVEZONE_LINK) && has_region)
    {
        iRet = unv_smart_intrusion_linkage_actions_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strcmp(configpath, UNV_SMART_LEAVEZONE_WEEKPLAN) && has_region)
    {
        iRet = unv_smart_intrusion_week_plan_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if (!strcmp(configpath, UNV_SMART_OBJTARCK_RULE) && has_track)
    {
        iRet = unv_smart_autotrack_rule_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    /////////////////////////通用订阅部分///////////////////////////////////////
    else if(!strcmp(configpath, UNV_SMART_WKSTAT))   //工作状态
    {
        iRet = unv_smart_work_status_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SYSTEM_PHOTO))
    {
        iRet = unv_system_photoserver_set(pInst, http_url, pMsgBody, pCbResponse);
    }
    else if(!strcmp(configpath, UNV_SMART_ATTCLL))  //Set_AttributeCollect
    {
        iRet = unv_smart_attribute_collect_set(pInst, http_url, pMsgBody, pCbResponse );
    }
    else if(!strncmp(configpath, UNV_SUBCRIPTION_X, 13))    //订阅刷新
    {
        if(strstr(msgbuf, "DELETE") != NULL)
        {
            iRet = unv_subcription_delete_handle(pInst, http_url, pMsgBody, pCbResponse);
            //g_link_endtime = GetCurrentTimeStamp();
        }
        else
        {
            //pthread_mutex_lock(&g_mclock);
            iRet = unv_subcription_refresh_handle(pInst, http_url, pMsgBody, pCbResponse );
            //pthread_mutex_unlock(&g_mclock);
        }
    }
    else if(!strncmp(configpath, UNV_SUBCRIPTION, 12))//******************************
    {
        iRet = unv_subcription_handle(pInst, http_url, pMsgBody, pCbResponse );
    }
    else
    {
        __ERR("!!!!!!! 404 unv put url:%s !!!!!!!\n", configpath);
        if( NULL != pCbResponse)
        {
            pCbResponse(pInst, "", 404);
        }
    }

    return iRet;
}

/*  
 *	处理宇视NVR get请求
 */
int unv_handle_get_request(const char *http_url, char **pResponseBuffer)
{
    __DBG("unv get request\n");
    if(http_url == NULL || pResponseBuffer == NULL)
    {
        return -1;
    }

    int status = 0;

    char prefixstr1[64] = {0};
    char prefixstr2[64] = {0};
    snprintf(prefixstr1, sizeof(prefixstr1), UNV_LAPI_SYSTEM);
    snprintf(prefixstr2, sizeof(prefixstr2), UNV_LAPI_CHANNELS);

    char channelid[32] = {0};

    char *configpath = NULL;
    char *str = NULL;
    int strlength = 0;

    // 首先判断前缀/LAPI/V1.0/Channels        或  /LAPI/V1.0/System则无通道
    if ((strncmp(http_url, prefixstr2, strlen(prefixstr2))) == 0)
    {
        // 获取通道ID 对于IPC, 通道ID无效
        if((str = strstr(http_url + strlen(prefixstr2) + 1, "/")) == NULL)
        {
            __ERR("don't get chn id\n");
            return -1;
        }
        strlength = str - (http_url + strlen(prefixstr2) + 1);
        memcpy(channelid, http_url + strlen(prefixstr2) + 1, strlength);
    }
    else if((strncmp(http_url, prefixstr1, strlen(prefixstr1))) == 0)
    {
        if((str = strstr(http_url + strlen("/LAPI/V1.0"), "/")) == NULL)
        {
            __ERR("don't get comm prefix\n");
            return -1;
        }
        strlength = str - (http_url + strlen(prefixstr1) + 1);
    }
    else
    {
        __ERR("don't get prefix\n");
        return -1;
    }

    //匹配url
    configpath = str + 1;

    int responsebuflen = 25 * 1024;
    char *responsebuf = (char *)anj_mw_malloc(responsebuflen);
    if(responsebuf == NULL)
    {	
        return -1;
    }	
    else
    {
        memset(responsebuf, 0, responsebuflen);
    }

    http_alarm_ability_t *pstAbility = http_alarm_ability_get();
    int has_smart = pstAbility->smart_motion;
    int has_human = pstAbility->human_enable;
    int has_car = pstAbility->car_enable;
    int has_gate = pstAbility->video_gate;
    int has_face = pstAbility->face_detect;
    int has_region = pstAbility->region_enable;
    int has_track = pstAbility->auto_track;

    cJSON *jsonbuf = NULL;
    char *jsonbuf_pri = NULL;

    ////////////////////////////////////////人型-老版本//////////////////////////////////////////////
    //获取报警能力集
    if(!strcmp(UNV_ALARM_CAPPABILITY, configpath) && 0)
    {
        //Check_yen_version_state(1);
        unv_alarm_capability_get(&jsonbuf);
    }
    //人形检测的规则使能信息
    else if(!strcmp(UNV_ALARM_HUMAN_RULE, configpath) && 0)
    {
        unv_alarm_human_rule_get(&jsonbuf);
    }
    //人形检测的所有矩形区域信息
    else if(!strcmp(UNV_ALARM_HUMAN_AREAS, configpath) && 0)
    {
        unv_alarm_human_areas_get(channelid, &jsonbuf);
    }	
    //人形检测的布防计划
    else if(!strcmp(UNV_ALARM_HUMAN_WEEKPLAN, configpath) && 0 && has_human)
    {
        unv_alarm_human_week_plan_get(&jsonbuf);
    }
    ////////////////////////////////////////通用定制//////////////////////////////////////////////
    //获取音频能力集合
    else if(!strcmp(UNV_MEDIA_AUDIO_CAP, configpath))
    {
        unv_media_audio_capability_get(&jsonbuf);
    }
    //获取音频input
    else if(!strcmp(UNV_MEDIA_AUDIO_INPUT, configpath))
    {
        unv_media_audio_input_get(&jsonbuf);
    }
    //获取音频output
    else if(!strcmp(UNV_MEDIA_AUDIO_OUTPUT, configpath))
    {
        unv_media_audio_output_get(&jsonbuf);
    }
    //获取系统基本的能力集**
    else if(!strcmp(UNV_SYSTEM_CAP, configpath))
    {
        unv_system_capability_get(&jsonbuf);
    }
    //获取系统图片服务器
    else if(!strcmp(UNV_SYSTEM_PHOTO, configpath))
    {
        unv_system_photoserver_get(&jsonbuf);
    }
    //获取系统报警音频文件
    else if(!strcmp(UNV_SYSTEM_AUDIOFILE, configpath))
    {
        unv_system_audiofile_get(&jsonbuf);
    }
    //获取互斥关系**
    else if(!strcmp(UNV_SMART_MUXR, configpath))
    {
        unv_smart_mutex_relation_info_get(&jsonbuf);
    }
    //获取智能工作状态**
    else if(!strcmp(UNV_SMART_WKSTAT, configpath))
    {
        unv_smart_working_status_info_get(&jsonbuf);
    }
    //获取车形检测的能力集 or 智能移动能力集**
    else if(!strcmp(UNV_SMART_CAP, configpath))
    {
        if (0)  //支持老版本则用包含 Vehicle的能力集
            unv_smart_capability_get(&jsonbuf); //新版本也带上车型能力集,后端没影响
        else
            unv_smart_capability_ex_get(&jsonbuf);
    }
    //获取图像能力集
    else if(!strcmp(UNV_IMAGE_CAPABILITIES, configpath))
    {
        unv_image_capability_get(&jsonbuf);
    }
    //获取图像配置
    else if(!strcmp(UNV_IMAGE_ENHANCE, configpath))
    {
        unv_image_enhance_get(&jsonbuf);
    }
    //获取灯光配置
    else if(!strcmp(UNV_IMAGE_LAMPCTRL, configpath))
    {
        unv_image_lampctrl_get(&jsonbuf);
    }
    /////////////////////////////////车型-老版本//////////////////////////////////////////////////////////
    //获取车形规则的能力集,识别老版本
    else if(!strcmp(UNV_SMART_VEHICLE_DETECT_RULE, configpath) && 0 && has_car)
    {
        unv_smart_vehicle_rule_get(&jsonbuf);
    }
    //获取车形所有矩形区域信息
    else if(!strcmp(UNV_SMART_VEHICLE_DETECT_AREAS, configpath) && 0 && has_car)
    {
        unv_smart_vehicle_areas_get(&jsonbuf);
    }
    //获取车形单个矩形区域信息
    else if(!strcmp(UNV_SMART_VEHICLE_DETECT_AREAS0, configpath) && 0 && has_car)
    {
        unv_smart_vehicle_areas0_get(&jsonbuf);
    }
    //获取车形布防计划
    else if(!strcmp(UNV_SMART_VEHICLE_WEEKPLAN, configpath) && 0 && has_car)
    {
        unv_alarm_human_week_plan_get(&jsonbuf);
    }
    /////////////////////////////////定制智能移动侦测-Smart -AMD/////////////////////////////////////////////////////
    //获取SMOTION_AreaType
    //else if(!strcmp(ALARM_MOTION_AREATYPE, configpath))
    //{
    //	Get_Alarm_SmartMotionRuleInfo(jsonbuf);
    //}
    //获取智能移动侦测规则**
    else if(!strcmp(UNV_ALARM_MOTION_RULE, configpath) && has_smart)
    {
        unv_alarm_smart_motion_rule_get(&jsonbuf);
    }
    //获取智能移动侦测全部区域**
    else if(!strcmp(UNV_ALARM_MOTION_AREAS, configpath) && has_smart)
    {
        unv_alarm_smart_motion_areas_get(&jsonbuf);
    }
    //获取智能移动侦测单个区域**
    else if(!strcmp(UNV_ALARM_MOTION_AREAS0, configpath) && has_smart)
    {
        unv_alarm_smart_motion_areas0_get(&jsonbuf);
    }
    //获取智能移动侦测联动信息**
    else if(!strcmp(UNV_ALARM_MOTION_LINK, configpath) && has_smart)
    {
        unv_alarm_smart_motion_linkage_action_get(&jsonbuf);
    }
    //获取智能移动侦测布防计划**
    else if(!strcmp(UNV_ALARM_MOTION_WEEKPLAN, configpath) && has_smart)
    {
        int motion_mode = unv_motion_detect_mode_get();
        if (motion_mode == 0)
            unv_alarm_smart_motion_week_plan_get(&jsonbuf);
        else
            unv_alarm_human_week_plan_get(&jsonbuf);
    }
    /////////////////////////////////人脸-FACE-FACE//////////////////////////////////////////////////////////
    else if(!strcmp(UNV_SMART_FACE_ENABLE, configpath) && has_face)
    {
        unv_smart_facedetect_enable_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_FACE_RULE, configpath) && has_face)
    {
        unv_smart_facedetect_rule_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_FACE_AREAS, configpath) && has_face)
    {
        unv_smart_facedetect_areas_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_FACE_WEEKPLAN, configpath) && has_face)
    {
        unv_smart_facedetect_week_plan_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_FACE_LINK, configpath) && has_face)
    {
        unv_smart_facedetect_linkage_action_get(&jsonbuf);
    }
    /////////////////////////////////拌线越界-GATE-CLD//////////////////////////////////////////////////////////
    else if(!strcmp(UNV_SMART_CROSSLINE_RULE, configpath) && has_gate)
    {
        unv_smart_crossline_rule_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_AREAS0, configpath) && has_gate)
    {
        unv_smart_crossline_one_area_get(&jsonbuf, 0);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_AREAS1, configpath) && has_gate)
    {
        unv_smart_crossline_one_area_get(&jsonbuf, 1);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_AREAS2, configpath) && has_gate)
    {
        unv_smart_crossline_one_area_get(&jsonbuf, 2);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_AREAS3, configpath) && has_gate)
    {
        unv_smart_crossline_one_area_get(&jsonbuf, 3);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_AREAS, configpath) && has_gate)
    {
        unv_smart_crossline_areas_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_LINK, configpath) && has_gate)
    {
        unv_smart_crossline_linkage_action_get(&jsonbuf);
    }
    else if(!strcmp(UNV_SMART_CROSSLINE_WEEKPLAN, configpath) && has_gate)
    {
        unv_smart_crossline_week_plan_get(&jsonbuf);
    }
    /////////////////////////////////区域入侵暂未调用-REGION-Intrusion//////////////////////////////////////////////////////////
    //获取区域入侵规则**
    else if(!strcmp(UNV_SMART_INTRU_RULE, configpath) && has_region)
    {
        unv_smart_intrusion_rule_get(&jsonbuf, 0);
    }
    //获取区域入侵矩形信息**
    else if(!strcmp(UNV_SMART_INTRU_AREAS, configpath) && has_region)
    {
        unv_smart_intrusion_areas_get(&jsonbuf, 0);
    }
    //获取区域入侵单个矩形信息**
    else if(!strcmp(UNV_SMART_INTRU_AREAS0, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 0, 0);
    }
    else if(!strcmp(UNV_SMART_INTRU_AREAS1, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 0, 1);
    }
    else if(!strcmp(UNV_SMART_INTRU_AREAS2, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 0, 2);
    }
    else if(!strcmp(UNV_SMART_INTRU_AREAS3, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 0, 3);
    }
    //获取区域入侵联动信息**
    else if(!strcmp(UNV_SMART_INTRU_LINK, configpath) && has_region)
    {
        unv_smart_intrusion_linkage_action_get(&jsonbuf);
    }
    //获取区域入侵布防计划**
    else if(!strcmp(UNV_SMART_INTRU_WEEKPLAN, configpath) && has_region)
    {
        unv_smart_intrusion_week_plan_get(&jsonbuf);
    }
    /////////////////////////////////进入区域暂未调用-REGION-AccessZone//////////////////////////////////////////////////////////
    //获取进入区域规则**
    else if(!strcmp(UNV_SMART_ACCESSZONE_RULE, configpath) && has_region)
    {
        unv_smart_intrusion_rule_get(&jsonbuf, 1);
    }
    //获取进入区域矩形信息**
    else if(!strcmp(UNV_SMART_ACCESSZONE_AREAS, configpath) && has_region)
    {
        unv_smart_intrusion_areas_get(&jsonbuf, 1);
    }
    //获取进入区域单个矩形信息**
    else if(!strcmp(UNV_SMART_ACCESSZONE_AREAS0, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 1, 0);
    }
    else if(!strcmp(UNV_SMART_ACCESSZONE_AREAS1, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 1, 1);
    }
    else if(!strcmp(UNV_SMART_ACCESSZONE_AREAS2, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 1, 2);
    }
    else if(!strcmp(UNV_SMART_ACCESSZONE_AREAS3, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 1, 3);
    }
    //获取进入区域联动信息**
    else if(!strcmp(UNV_SMART_ACCESSZONE_LINK, configpath) && has_region)
    {
        unv_smart_intrusion_linkage_action_get(&jsonbuf);
    }
    //获取进入区域布防计划**
    else if(!strcmp(UNV_SMART_ACCESSZONE_WEEKPLAN, configpath) && has_region)
    {
        unv_smart_intrusion_week_plan_get(&jsonbuf);
    }
    /////////////////////////////////离开区域暂未调用-REGION-LeaveZone//////////////////////////////////////////////////////////
    //获取离开区域规则**
    else if(!strcmp(UNV_SMART_LEAVEZONE_RULE, configpath) && has_region)
    {
        unv_smart_intrusion_rule_get(&jsonbuf, 2);
    }
    //获取离开区域矩形信息**
    else if(!strcmp(UNV_SMART_LEAVEZONE_AREAS, configpath) && has_region)
    {
        unv_smart_intrusion_areas_get(&jsonbuf, 2);
    }
    //获取离开区域单个矩形信息**
    else if(!strcmp(UNV_SMART_LEAVEZONE_AREAS0, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 2, 0);
    }
    else if(!strcmp(UNV_SMART_LEAVEZONE_AREAS1, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 2, 1);
    }
    else if(!strcmp(UNV_SMART_LEAVEZONE_AREAS2, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 2, 2);
    }
    else if(!strcmp(UNV_SMART_LEAVEZONE_AREAS3, configpath) && has_region)
    {
        unv_smart_intrusion_one_area_get(&jsonbuf, 2, 3);
    }
    //获取离开区域联动信息**
    else if(!strcmp(UNV_SMART_LEAVEZONE_LINK, configpath) && has_region)
    {
        unv_smart_intrusion_linkage_action_get(&jsonbuf);
    }
    //获取离开区域布防计划**
    else if(!strcmp(UNV_SMART_LEAVEZONE_WEEKPLAN, configpath) && has_region)
    {
        unv_smart_intrusion_week_plan_get(&jsonbuf);
    }
    ///////////////////////////////// 自动跟踪-autotrack //////////////////////////////////////////////////////////
    else if (!strcmp(UNV_SMART_OBJTARCK_RULE, configpath) && has_track)
    {
        unv_smart_autotrack_rule_get(&jsonbuf);
    }
    /////////////////////////////////404//////////////////////////////////////////////////////////
    else
    {
        __ERR("!!!!!!! 404 unv get url:%s !!!!!!!\n", configpath);
        status = HTTP_RES_STATUS_NOT_FOUND;
        snprintf(responsebuf, responsebuflen, unv_str_response, http_url, "\"null\"");
        goto __EXIT;
    }

    jsonbuf_pri = cJSON_Print(jsonbuf);
    snprintf(responsebuf, responsebuflen, unv_str_response, http_url, jsonbuf_pri);

    //返回 200 代表成功
    status = HTTP_RES_STATUS_OK;

__EXIT:
    *pResponseBuffer = responsebuf;

    if(jsonbuf_pri != NULL)
    {
        anj_mw_free(jsonbuf_pri);
        jsonbuf_pri = NULL;
    }

    if(jsonbuf != NULL)
    {
        cJSON_Delete(jsonbuf);
        jsonbuf = NULL;
    }

    //返回0 代表不做任何的处理
    return status;
}

static int http_unv_start(void)
{
    int iRet = 0;
    MediaStreamConfig *pStream = (MediaStreamConfig *)getMediaStreamConfig();

    g_onvif_expand = pStream->unvConfig.onvif_expand;
    g_yen_version_smd = 0;
    g_yen_version_sdm = 0;

    memset(&s_stAnjUnvInfo, 0, sizeof(unv_info_t));
    s_stAnjUnvInfo.unv_enable = pStream->unvConfig.onvif_expand;
    s_stAnjUnvInfo.plug_by_play_support = pStream->unvConfig.onvif_expand;
    s_stAnjUnvInfo.smart_support = pStream->unvConfig.smart_nvr;
    s_stAnjUnvInfo.private_probe_support = pStream->unvConfig.privatetype;

    __INFO("unv ability: enable:%d, smart:%d, plug_by_play:%d, private_probe:%d!\n",
        s_stAnjUnvInfo.unv_enable, s_stAnjUnvInfo.smart_support,
        s_stAnjUnvInfo.plug_by_play_support, s_stAnjUnvInfo.private_probe_support);

    if (s_stAnjUnvInfo.unv_enable == 0)
    {
        __ERR("device don't support unv protocol!\n");
        return -1;
    }

    if (s_stAnjUnvInfo.private_probe_support)
    {
        memset(&s_stUnvSearchTimingThread, 0, sizeof(anj_thread_s));
        s_stUnvSearchTimingThread.bAutoDestroy = 1;
        strncpy(s_stUnvSearchTimingThread.iThreadName, "unv_search_timing_thread", sizeof(s_stUnvSearchTimingThread.iThreadName) - 1);
        s_stUnvSearchTimingThread.iThreadjob.ctx = &s_stUnvSearchTimingThread;
        s_stUnvSearchTimingThread.iThreadjob.func = unv_search_timing_thread;
        iRet = anj_thread_task_create(&s_stUnvSearchTimingThread);
        if (iRet)
        {
            __ERR("unv_search_timing_thread create failed\n");
        }
    }

    eventhub_subscribe(EVENTHUB_CLASS_STATUS, (char *)EVENTHUB_HTTP_UNV_GET_PROBE_STATUS, unv_search_status_get);
    return iRet;
}

static void http_unv_stop(void)
{
    eventhub_unsubscribe(EVENTHUB_CLASS_STATUS, EVENTHUB_HTTP_UNV_GET_PROBE_STATUS, unv_search_status_get);

    if (s_stUnvSearchTimingThread.start == 1)
    {
        anj_thread_task_destroy(&s_stUnvSearchTimingThread, -1);
    }
}

static void http_unv_restart(EventResult *event_result, void *data)
{
    (void)data;
    if (event_result)
    {
        event_result->ret = 0;
    }

    __INFO("unv restart\n");
    http_unv_stop();
    usleep(500 * 1000);
    http_unv_start();
}

int http_unv_init()
{
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_UNV_RESTART, http_unv_restart);
    return http_unv_start();
}

void http_unv_uninit()
{
    http_unv_stop();
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, EVENTHUB_UNV_RESTART, http_unv_restart);
}
