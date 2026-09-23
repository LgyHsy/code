#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_log.h"

#include "anj_config.h"
#include "alarm_link.h"
#include "cJSON.h"

#include "http_def.h"
#include "http_handle.h"
#include "http_unv.h"
#include "unv_def.h"
#include "unv_subs.h"
#include "unv_put.h"

static unsigned int s_unv_sub_id_num = 0;

static char ResponseOKStr[] = 
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

static char ResponseFailStr[] = 
"{\r\n"
"\"Response\": {\r\n"
"\t\"ResponseURL\":\"%s\",\r\n"
"\t\"CreatedID\":-1,\r\n"
"\t\"ResponseCode\":-1,\r\n"
"\t\"SubResponseCode\":0,\r\n"
"\t\"ResponseString\":\"Failed\",\r\n"
"\t\"StatusCode\":%d,\r\n"
"\t\"StatusString\":\"Failed\",\r\n"
"\t\"Data\":%s\r\n"
"\t}\r\n"
"}\r\n";


int ToInt(const char *pbin)
{
    int ii = 0;
    int result = 0;

    while (pbin[ii]!=0)
    {
        result = result * 2 + (pbin[ii] - '0');
        ii++;
    }

    return result;
}

int ToBin(int a, char *buf)
{
    int i = 31;
    int r = 0;
    int s = 0;

    char stack[33] = {'0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0',
        '0','0','0','0','0','0','0','0'};

    do 
    {
        r = a / 2;
        s = a % 2;
        if (s == 0)
        {
            stack[i] = '0';
        }
        else
        {
            stack[i] = '1';
        }

        if (r != 0)
        {
            --i;
            a = r;
        }
    } while (r);

    memcpy(buf, stack, sizeof(stack) - 1);
    return 0;
}

int Set_one_zero(int B, int E, char *plan)
{
    int i;
    for (i = 0; i < (E-B); i++)
        plan[31 - B - i] = 0x31;

    return HTTP_PUT_OK;
}

int Get_time(char *time)
{
    int i = 0;
    char time_buf[3] = {0};
    char *p = time;
    p++;

    while(*p != ':')
    {
        time_buf[i] = *p;
        i++;
        p++;
    }

    return atoi(time_buf);
}

int alarm_time_section_set(int ID, int Num, cJSON *TimeSectionInfos, TimeSpanCfg *pTimeSpan)
{
    int i, B, E, T1, T2;
    char *BeginStr = NULL;
    char *EndStr = NULL;
    char plan[33] = {'0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', '\0'};

    for (i = 0; i < Num; i++)
    {
        cJSON *TimeSectionInfo = cJSON_GetArrayItem(TimeSectionInfos, i);
        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfo, "Begin");
        cJSON *End = cJSON_GetObjectItem(TimeSectionInfo, "End");
        //cJSON *ArmingType  = cJSON_GetObjectItem(TimeSectionInfo, "ArmingType");

        BeginStr = Begin->valuestring;// free 3
        sscanf(BeginStr, "%d:%d:%d", &B, &T1, &T2);
        //B = Get_time(B_pri); 
        EndStr = End->valuestring; // free 4

        if (!strcmp(EndStr, "23:59:59"))
            E = 24;
        else
            sscanf(EndStr, "%d:%d:%d", &E, &T1, &T2);

        __DBG("BeginStr:%s, EndStr:%s, B:%d, E:%d\n", BeginStr, EndStr, B, E);
        Set_one_zero(B, E, plan); 
    }

    if (ID == 7)
    {
        pTimeSpan->workday[0] = ToInt(plan);
    }
    else
    {
        pTimeSpan->workday[ID] = ToInt(plan);
    }

    return 0;
}

int alarm_day_plan_set(cJSON *DayPlan, TimeSpanCfg *pTimeSpan)
{
    __DBG("alarm day plan set\n");
    //int i;

    cJSON *ID = cJSON_GetObjectItem(DayPlan, "ID");
    cJSON *Num  = cJSON_GetObjectItem(DayPlan, "Num");
    cJSON *TimeSectionInfos = cJSON_GetObjectItem(DayPlan, "TimeSectionInfos");

    int ID_int = ID->valueint;
    int Num_int = Num->valueint;

    if (TimeSectionInfos != NULL)
    {
        alarm_time_section_set(ID_int, Num_int, TimeSectionInfos, pTimeSpan);
    }

    return 0;
}

/*灯光控制*/
int unv_image_lampctrl_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv image lampctrl set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);      //free 1
    cJSON *pImageLampctrlJson = cJSON_Parse(pMsgBody);      //free 2
    if(Response == NULL || pImageLampctrlJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pVideoCapCfg = &pMediaCfg->videoConfig[0].videoCapture;

    VideoCaptureCfg stVideoCapCfg = {0};
    memcpy(&stVideoCapCfg, pVideoCapCfg, sizeof(VideoCaptureCfg));

    cJSON *Enabled = cJSON_GetObjectItem(pImageLampctrlJson, "Enabled");
    cJSON *Type = cJSON_GetObjectItem(pImageLampctrlJson, "Type");
    cJSON *Mode = cJSON_GetObjectItem(pImageLampctrlJson, "Mode");
    cJSON *NearLevel = cJSON_GetObjectItem(pImageLampctrlJson, "NearLevel");

    if (Enabled != NULL)
    {
        int enable  = Enabled->valueint;
        __DBG("lampctrl enable:%d\n", enable);
        if (enable == 0)
        {
            pVideoCapCfg->led_brightness_mode = LED_BRIGHTNESS_MODE_MANUAL;
            pVideoCapCfg->led_brightness_value = 0;
        }
    }

    if (Type != NULL)
    {
        int type  = Type->valueint;
        __DBG("lampctrl type:%d\n", type);
        if (type == 2)
        {
            pVideoCapCfg->led_mode = 0; //red
            pVideoCapCfg->ircut_keepcolor = 0;
        }
        else if(type == 1)
        {
            pVideoCapCfg->led_mode = 1;//white
            pVideoCapCfg->ircut_keepcolor = 1;
        }
        else if(type == 6)
        {
            pVideoCapCfg->led_mode = 2;//double
        }
        else
        {
            pVideoCapCfg->led_mode = 2;
        }
    }

    if (Mode != NULL)
    {
        int mode  = Mode->valueint;
        __DBG("lampctrl mode:%d\n", mode);
        if (mode == 0)
            pVideoCapCfg->ircut_mode = 0;
        else if (mode == 4)
            pVideoCapCfg->ircut_mode = 1;
        else if(mode == 5)
            pVideoCapCfg->ircut_mode = 2;
        else if(mode == 7)
            pVideoCapCfg->ircut_mode = 3;
        else
            pVideoCapCfg->ircut_mode = 3;
    }

    if (NearLevel != NULL)
    {
        int near_level  = NearLevel->valueint;
        pVideoCapCfg->led_brightness_value = (near_level / 100) * 10;
        __DBG("videocapture led_brightness_value:%d, near_level:%d!\n", pVideoCapCfg->led_brightness_value, near_level);
    }

    if (memcmp(&stVideoCapCfg, pVideoCapCfg, sizeof(VideoCaptureCfg)) != 0)
    {
        int cameraIndex = 0;
        for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
        {
            anj_config_video_capture_set(pVideoCapCfg, cameraIndex);
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pImageLampctrlJson != NULL)
    {
        cJSON_Delete(pImageLampctrlJson);
        pImageLampctrlJson = NULL;
    }

    return HTTP_PUT_OK;
}


//设置图像--翻转
int unv_image_enhance_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv image enhance set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg stVideoCapConfig = {0};
    memcpy(&stVideoCapConfig, &pMediaCfg->videoConfig[iCameraIdx].videoCapture, sizeof(VideoCaptureCfg));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);      //free 1
    cJSON *pImageEnhanceJson = cJSON_Parse(pMsgBody);   //free 2

    if(Response == NULL || pImageEnhanceJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }
    __DBG("pMsgBody:%s\n", pMsgBody);

    cJSON *SourceType = cJSON_GetObjectItem(pImageEnhanceJson, "SourceType");
    cJSON *Brightness = cJSON_GetObjectItem(pImageEnhanceJson, "Brightness");
    cJSON *Contrast = cJSON_GetObjectItem(pImageEnhanceJson, "Contrast");
    cJSON *Saturation = cJSON_GetObjectItem(pImageEnhanceJson, "Saturation");
    cJSON *Sharpness = cJSON_GetObjectItem(pImageEnhanceJson, "Sharpness");
    cJSON *ImageRotation = cJSON_GetObjectItem(pImageEnhanceJson, "ImageRotation");

    if (SourceType != NULL)
    {
        int SourceType_int = SourceType->valueint;
        stVideoCapConfig.led_mode = SourceType_int;
    }
    if (Brightness != NULL)
    {
        int Brightness_int = Brightness->valueint;
        stVideoCapConfig.brightness = Brightness_int;
    }
    if (Contrast != NULL)
    {
        int Contrast_int = Contrast->valueint;
        stVideoCapConfig.contrast = Contrast_int;
    }
    if (Saturation != NULL)
    {
        int Saturation_int = Saturation->valueint;
        stVideoCapConfig.saturation = Saturation_int;
    }
    if (Sharpness != NULL)
    {
        int Sharpness_int = Sharpness->valueint;
        stVideoCapConfig.sharpness = Sharpness_int;
    }
    if (ImageRotation != NULL)
    {
        int ImageRotation_int = ImageRotation->valueint;
        if (ImageRotation_int == 0)
        {
            stVideoCapConfig.hflip = 0;
            stVideoCapConfig.vflip = 0;
        }
        else if (ImageRotation_int == 1)
        {
            stVideoCapConfig.hflip = 1;
            stVideoCapConfig.vflip = 0;
        }
        else if (ImageRotation_int == 2)
        {
            stVideoCapConfig.hflip = 0;
            stVideoCapConfig.vflip = 1;
        }
        else if (ImageRotation_int == 3)
        {
            stVideoCapConfig.hflip = 1;
            stVideoCapConfig.vflip = 1;
        }
    }

    {
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            anj_config_video_capture_set(&stVideoCapConfig, iCameraIdx);
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pImageEnhanceJson != NULL)
    {
        cJSON_Delete(pImageEnhanceJson);
        pImageEnhanceJson = NULL;
    }

    return HTTP_PUT_OK;
}


//设置Audio
int unv_media_audio_output_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv media audio output set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture stAudioCapture;
    memcpy(&stAudioCapture, &pMediaCfg->audioConfig.audioCapture, sizeof(AudioCapture));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);  //free 1
    cJSON *pAudioOutJson = cJSON_Parse(pMsgBody);  //free 2
    if(Response == NULL || pAudioOutJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    __DBG("MsgBody:%s\n", pMsgBody);

    cJSON *Gain = cJSON_GetObjectItem(pAudioOutJson, "Gain");
    if (Gain != NULL)
    {
        stAudioCapture.volume_play = (int)round(Gain->valueint / 2.55);
    }

    {
        anj_config_audio_capture_set(&stAudioCapture);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if ( NULL != pAudioOutJson )
    {
        cJSON_Delete(pAudioOutJson);
        pAudioOutJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_media_audio_input_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv media audio input set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig stAudioConfig = {0};
    memcpy(&stAudioConfig, &pMediaCfg->audioConfig, sizeof(AudioConfig));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);      //free 1
    cJSON *pAudioInputJson = cJSON_Parse(pMsgBody);       //free 2

    if(Response == NULL || pAudioInputJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }
    __DBG("pMsgBody:%s\n", pMsgBody);

    cJSON *IsMute = cJSON_GetObjectItem(pAudioInputJson, "IsMute");
    cJSON *EncodeFormat  = cJSON_GetObjectItem(pAudioInputJson, "EncodeFormat");
    cJSON *SampleRate  = cJSON_GetObjectItem(pAudioInputJson, "SampleRate");
    cJSON *InputGain  = cJSON_GetObjectItem(pAudioInputJson, "InputGain");
    cJSON *AudioInputList  = cJSON_GetObjectItem(pAudioInputJson, "AudioInputList");

    int IsMute_value = 0;
    int AudioInput_Enable_value = 0;

    if (IsMute != NULL)
    {
        if (IsMute->valueint == 1)
            IsMute_value = 0;
        else if (IsMute->valueint == 0)
            IsMute_value = 1;
    }

    if (EncodeFormat != NULL)
    {
        memset(stAudioConfig.audioEncode.audioEncodeType.typeName, 0, AUDIO_ENCODE_TYPE_MAX_LEN);
        if (EncodeFormat->valueint == 1)
        {
            stAudioConfig.audioEncode.bitRate = 64000;
            strcpy(stAudioConfig.audioEncode.audioEncodeType.typeName, "G.711A");
        }
        else if (EncodeFormat->valueint == 2)
        {
            stAudioConfig.audioEncode.bitRate = 64000;
            strcpy(stAudioConfig.audioEncode.audioEncodeType.typeName, "G.711");
        }
        else if (EncodeFormat->valueint == 6)
        {
            stAudioConfig.audioEncode.bitRate = 16000;
            strcpy(stAudioConfig.audioEncode.audioEncodeType.typeName, "AAC");
        }
        else
        {
            strcpy(stAudioConfig.audioEncode.audioEncodeType.typeName, "G.711");
        }
    }

    if (SampleRate != NULL)
    {
        if (SampleRate->valueint == 1)
            stAudioConfig.audioEncode.sampleRate = 16000;
        else
            stAudioConfig.audioEncode.sampleRate = 8000;
    }

    if (InputGain != NULL)
    {
        stAudioConfig.audioCapture.volume_capture = (int)round(InputGain->valueint /2.55);
    }

    if (AudioInputList != NULL)
    {
        if (cJSON_GetArraySize(AudioInputList) > 0)
        {
            cJSON *AudioInputJson  = cJSON_GetArrayItem(AudioInputList, 0);
            if (AudioInputJson != NULL)
            {
                cJSON *Enabled = cJSON_GetObjectItem(AudioInputJson, "Enabled");
                if (Enabled->valueint == 0)
                    AudioInput_Enable_value = 0;
                else if (Enabled->valueint == 1)
                    AudioInput_Enable_value = 1;
            }
        }
    }

    if (AudioInput_Enable_value == 0 && IsMute_value == 0)
    {
        stAudioConfig.audioEncode.enable = 0;
    }
    else
    {
        stAudioConfig.audioEncode.enable = 1;
    }

    {
        anj_config_audio_set(&stAudioConfig);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAudioInputJson != NULL)
    {
        cJSON_Delete(pAudioInputJson);
        pAudioInputJson = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv face detect set *************************/

int smart_facedetect_rect_area_set(cJSON *FaceDetectionPolygonInfo)
{
    __DBG("smart facedetectt rect area_set\n");

    int i = 0;
    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if(pstNewFaceDetectAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewFaceDetectAlarm, &pAlarmCfg->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

    //cJSON *ID         = cJSON_GetObjectItem(FaceDetectionPolygonInfo, "ID");
    cJSON *Enabled      = cJSON_GetObjectItem(FaceDetectionPolygonInfo, "Enabled");
    cJSON *Sensitivity  = cJSON_GetObjectItem(FaceDetectionPolygonInfo, "Sensitivity");
    cJSON *Points       = cJSON_GetObjectItem(FaceDetectionPolygonInfo, "Points");
    cJSON *PointNum     = cJSON_GetObjectItem(FaceDetectionPolygonInfo, "PointNum");

    if (Enabled != NULL)
    {
        int enable = Enabled->valueint;
        if (enable == 1) 
        {
            pstNewFaceDetectAlarm->enable = 1;
            pstNewFaceDetectAlarm->arming_flag = ARMING_ALLDAY;
        }
        else
        {
            pstNewFaceDetectAlarm->enable = 0;
            pstNewFaceDetectAlarm->arming_flag = ARMING_DISABLE;
        }
    }
    if (Sensitivity != NULL)
    {
        int Sensitivity_int = Sensitivity->valueint;
        if (Sensitivity_int >= 0 && Sensitivity_int <= 100)
        {
            pstNewFaceDetectAlarm->sensitivity = Sensitivity_int;
        }
    }
    if (Points != NULL && PointNum->valueint >= 3 && cJSON_GetArraySize(Points) >= 3)
    {
        int Point = PointNum->valueint;
        int x_min = 0, x_max = 0, y_min = 0, y_max = 0;

        cJSON *Point_t = cJSON_GetArrayItem(Points, 0);
        cJSON *X1 = cJSON_GetObjectItem(Point_t, "X");
        cJSON *Y1 = cJSON_GetObjectItem(Point_t, "Y");
        x_min = X1->valueint;
        x_max = X1->valueint;
        y_min = Y1->valueint;
        y_max = Y1->valueint;
        for (i = 1; i < Point; i++)
        {
            cJSON *Point_t = cJSON_GetArrayItem(Points, i);
            cJSON *X = cJSON_GetObjectItem(Point_t, "X");
            cJSON *Y = cJSON_GetObjectItem(Point_t, "Y");
            int x = X->valueint;
            int y = Y->valueint;

            CHECK_VALUE_LIMIT_RANGE(x, x_min, x_max);
            CHECK_VALUE_LIMIT_RANGE(y, y_min, y_max);
        }

        pstNewFaceDetectAlarm->area.xPos = x_min / 100;
        pstNewFaceDetectAlarm->area.yPos = y_min / 100;
        pstNewFaceDetectAlarm->area.width = (x_max - x_min) / 100;
        pstNewFaceDetectAlarm->area.height = (y_max - y_min) / 100;
    }

    {
        FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
        }
        anj_config_alarm_fd_set(stFaceAlarmArray);
    }

    if (pstNewFaceDetectAlarm)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    return 0;
}

int unv_smart_facedetect_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv face detect areas set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if (pstNewFaceDetectAlarm == NULL)
    {
        return -1;
    }

    int status = 200;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pFaceDetAreaListJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pFaceDetAreaListJson == NULL)
    {
        status = 304;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *pEntireImageEnabled = (cJSON_GetObjectItem(pFaceDetAreaListJson, "EntireImageEnabled"));
    cJSON *pPolygons = (cJSON_GetObjectItem(pFaceDetAreaListJson, "Polygons"));
    cJSON *pNum = (cJSON_GetObjectItem(pFaceDetAreaListJson, "Num"));

    if (pNum != NULL)
    {
        int num = pNum->valueint;
        if(num == 1) 
        {
            cJSON *FaceDetectionPolygonInfo = cJSON_GetArrayItem(pPolygons, 0);
            if (FaceDetectionPolygonInfo != NULL)
            {
                smart_facedetect_rect_area_set(FaceDetectionPolygonInfo); 
            }
        }
        else
        {
            status = 304;
            snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
            goto __EXIT;
        }
    }

    if (pEntireImageEnabled != NULL)
    {
        int EntireImageEnabled = pEntireImageEnabled->valueint;
        if (EntireImageEnabled == 1)
        {
            int iCameraIdx = 0;
            AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
            memcpy(pstNewFaceDetectAlarm, &pAlarmCfg->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

            pstNewFaceDetectAlarm->area.xPos = 0;
            pstNewFaceDetectAlarm->area.yPos = 0;
            pstNewFaceDetectAlarm->area.height = 100;
            pstNewFaceDetectAlarm->area.width = 100;

            {
                FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
                for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
                {
                    memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
                }
                anj_config_alarm_fd_set(stFaceAlarmArray);
            }
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewFaceDetectAlarm != NULL)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pFaceDetAreaListJson != NULL)
    {
        cJSON_Delete(pFaceDetAreaListJson);
        pFaceDetAreaListJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_facedetect_linkage_actions_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart facedetect link action set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if (pstNewFaceDetectAlarm == NULL)
    {
        return -1;
    }

    int status = 200;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pLinkageActionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pLinkageActionJson == NULL)
    {
        status = 304;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int num = cJSON_GetObjectItem(pLinkageActionJson, "Num")->valueint;
    if (num < 1)
    {
        status = 200;
        snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");
        goto __EXIT;
    }
    else
    {
        cJSON *Actions = cJSON_GetObjectItem(pLinkageActionJson, "Actions");
        int i = 0, j = 0;

        int iCameraIdx = 0;
        AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
        memcpy(pstNewFaceDetectAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

        MediaConfig *pstMediaConfig = (MediaConfig *)getMediaConfig();
        AudioCapture stAudioCapConfig = {0};
        memcpy(&stAudioCapConfig, &pstMediaConfig->audioConfig.audioCapture, sizeof(AudioCapture));

        AudioFileList *pstAudioList = http_audio_file_list_get();

        for (i = 0; i < num; i++)
        {
            cJSON *pLinkageActionInfo = cJSON_GetArrayItem(Actions, i);
            int ActID = cJSON_GetObjectItem(pLinkageActionInfo, "ActID")->valueint;
            __DBG("ActID:%d\n", ActID);

            if(ActID == 24)
            {
                int Enabled_int, WarnCount_int, AudioFileID_int, AudioVolume_int;

                cJSON *pActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (pActParam != NULL)
                {
                    cJSON *Enabled = cJSON_GetObjectItem(pActParam, "Enabled");
                    cJSON *WarnCount = cJSON_GetObjectItem(pActParam, "WarnCount");
                    cJSON *AudioFileID = cJSON_GetObjectItem(pActParam, "AudioFileID");
                    cJSON *AudioVolume = cJSON_GetObjectItem(pActParam, "AudioVolume");

                    if (Enabled != NULL)
                    {
                        Enabled_int = Enabled->valueint;
                        if (Enabled_int == 1)//原本状态不刷掉
                        {
                            if (pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_ALLDAY;
                        }
                        else
                            pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                    }

                    if (WarnCount != NULL)
                    {
                        WarnCount_int = WarnCount->valueint; 
                        pstNewFaceDetectAlarm->alarmAction.audioAction.times = WarnCount_int;
                    }

                    if (AudioFileID != NULL)
                    {
                        AudioFileID_int = AudioFileID->valueint;
                        if (AudioFileID_int < 10)
                        {
                            memset(&(pstNewFaceDetectAlarm->alarmAction.audioAction.filename), 0, sizeof(pstNewFaceDetectAlarm->alarmAction.audioAction.filename));
                            strcpy(pstNewFaceDetectAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                        }
                    }

                    if (AudioVolume != NULL)
                    {
                        AudioVolume_int = AudioVolume->valueint;
                        float vol_val = (float)AudioVolume_int;
                        vol_val = vol_val / 255 * 100;
                        stAudioCapConfig.volume_play = (int)vol_val;
                    }
                }
            }
            else if(ActID == 28)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                }

                __DBG("face detect audio action enable:%d, timespan_num:%u!\n", 
                    pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag,
                    pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timespan_num);

                cJSON *pActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (pActParam != NULL && Enabled_int != 0)
                {
                    cJSON *pAudioCustomInfo = cJSON_GetObjectItem(pActParam, "AudioCustomInfo");
                    if (pAudioCustomInfo != NULL)
                    {
                        cJSON *pDays = cJSON_GetObjectItem(pAudioCustomInfo, "Days");
                        if (pDays != NULL)
                        {
                            cJSON *pDays_1 = cJSON_GetArrayItem(pDays, 0);//获取周一
                            if (pDays_1 != NULL)
                            {
                                cJSON *pTimeSectionInfos = cJSON_GetObjectItem(pDays_1, "TimeSectionInfos");
                                if (pTimeSectionInfos != NULL)
                                {
                                    cJSON *pNum = cJSON_GetObjectItem(pDays_1, "Num");
                                    if (pNum != NULL)
                                    {
                                        int Num_int = pNum->valueint;
                                        if (Num_int == 4 && Enabled_int != 0)
                                        {
                                            pstNewFaceDetectAlarm->alarmAction.audioAction.enable.enable_flag = 4;

                                            int time_num = 0;
                                            for (j = 0; j < 4; j++)
                                            {
                                                cJSON *pTimeSectionInfos_i = cJSON_GetArrayItem(pTimeSectionInfos, j);
                                                if (pTimeSectionInfos_i != NULL)
                                                {
                                                    cJSON *Begin = cJSON_GetObjectItem(pTimeSectionInfos_i, "Begin");
                                                    cJSON *End = cJSON_GetObjectItem(pTimeSectionInfos_i, "End");
                                                    cJSON *AudioActParamInfo = cJSON_GetObjectItem(pTimeSectionInfos_i, "AudioActParamInfo");

                                                    if (AudioActParamInfo != NULL && j == 0)
                                                    {
                                                        cJSON *pWarnCount = cJSON_GetObjectItem(AudioActParamInfo, "WarnCount");
                                                        cJSON *pAudioFileID = cJSON_GetObjectItem(AudioActParamInfo, "AudioFileID");

                                                        if (pWarnCount != NULL)
                                                        {
                                                            int WarnCount_int = pWarnCount->valueint;
                                                            if (WarnCount_int > 3)
                                                                WarnCount_int = 3;
                                                            else if(WarnCount_int <= 0)
                                                                WarnCount_int = 1;

                                                            pstNewFaceDetectAlarm->alarmAction.audioAction.times = WarnCount_int;
                                                        }

                                                        if (pAudioFileID != NULL)
                                                        {
                                                            int AudioFileID_int = pAudioFileID->valueint;
                                                            if (AudioFileID_int < 10)
                                                            {
                                                                memset(&(pstNewFaceDetectAlarm->alarmAction.audioAction.filename), 0, sizeof(pstNewFaceDetectAlarm->alarmAction.audioAction.filename));
                                                                strcpy(pstNewFaceDetectAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                                                            }
                                                            __DBG("face detect audio action filename:%s, fileid:%d\n", pstNewFaceDetectAlarm->alarmAction.audioAction.filename, AudioFileID_int);
                                                        }
                                                    }

                                                    if (Begin != NULL && End != NULL)
                                                    {
                                                        char *pBeginStr = Begin->valuestring;
                                                        char *pEndStr = End->valuestring;

                                                        __DBG("Begin:%s, End:%s!\n", pBeginStr, pEndStr);

                                                        int bhour, bminute, bsec, ehour, eminute, esec;
                                                        int ret1 = sscanf(pBeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                        int ret2 = sscanf(pEndStr, "%d:%d:%d", &ehour, &eminute, &esec);

                                                        if (ret1 == 3 && ret2 == 3)
                                                        {
                                                            __DBG("Begin time: %d:%d:%d\n", bhour, bminute, bsec);
                                                            __DBG("End time: %d:%d:%d\n", ehour, eminute, esec);
                                                            //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                            {
                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.hour = bhour;
                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.minute = bminute;
                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.sec = bsec;

                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.hour = ehour;
                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.minute = eminute;
                                                                pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.sec = esec;
                                                                time_num ++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            __DBG("time_num:%d\n", time_num);
                                            pstNewFaceDetectAlarm->alarmAction.audioAction.enable.timespan_num = time_num;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        {
            FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
            }

            anj_config_alarm_fd_set(stFaceAlarmArray);
        }

        {
            anj_config_audio_capture_set(&stAudioCapConfig);
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewFaceDetectAlarm)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pLinkageActionJson != NULL)
    {
        cJSON_Delete(pLinkageActionJson);
        pLinkageActionJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_facedetect_enable_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)//使能智能侦测，关闭移动侦测
{
    __DBG("unv smart facedetect enable set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if (pstNewFaceDetectAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewFaceDetectAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);  //free 1
    cJSON *pFaceEnableJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pFaceEnableJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *pEnabled = cJSON_GetObjectItem(pFaceEnableJson, "Enabled");
    if (pEnabled != NULL)
    {
        int Enabled_int = pEnabled->valueint;
        if (Enabled_int == 1)
        {
            pstNewFaceDetectAlarm->enable = 1;
            pstNewFaceDetectAlarm->sensitivity = 6;
        }
        else
        {
            pstNewFaceDetectAlarm->enable = 0;
        }
    }

    {
        FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
        }

        anj_config_alarm_fd_set(stFaceAlarmArray);
    }


    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewFaceDetectAlarm)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pFaceEnableJson != NULL)
    {
        cJSON_Delete(pFaceEnableJson);
        pFaceEnableJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_facedetect_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)//使能智能侦测，关闭移动侦测
{
    __DBG("unv smart facedetect rule set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if (pstNewFaceDetectAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewFaceDetectAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pFaceDetectionRuleJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pFaceDetectionRuleJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *pFaceFullcheck = cJSON_GetObjectItem(pFaceDetectionRuleJson, "FaceFullcheck");
    if (pFaceFullcheck != NULL)
    {
        cJSON *pEnabled = cJSON_GetObjectItem(pFaceFullcheck, "Enabled");
        if (pEnabled != NULL)
        {
            int enable = pEnabled->valueint;
            if (enable == 1)
            {
                pstNewFaceDetectAlarm->enable = 1;
                pstNewFaceDetectAlarm->sensitivity = 6;
            }
            else
            {
                pstNewFaceDetectAlarm->enable = 0;
            }
        }
    }

    {
        FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
        }

        anj_config_alarm_fd_set(stFaceAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewFaceDetectAlarm)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pFaceDetectionRuleJson != NULL)
    {
        cJSON_Delete(pFaceDetectionRuleJson);
        pFaceDetectionRuleJson = NULL;
    }

    return HTTP_PUT_OK;
}

//设置星期
int unv_smart_facedetect_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart facedet week set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    FaceDetectAlarm *pstNewFaceDetectAlarm = (FaceDetectAlarm *) anj_mw_malloc(sizeof(FaceDetectAlarm));
    if (pstNewFaceDetectAlarm == NULL)
    {
        __ERR("pstNewFaceDetectAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewFaceDetectAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); // free 2
    if(Response == NULL || pWeekPlanJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int i = 0;
    int Num_int;
    int Enabled_int;

    cJSON *pEnabled = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    cJSON *pNum = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    cJSON *pDays = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    Enabled_int = pEnabled->valueint;
    Num_int = pNum->valueint;
    __DBG("enable:%d, num:%d\n", Enabled_int, Num_int);

    if (Enabled_int == 0) 
    {
        pstNewFaceDetectAlarm->arming_flag = 0;
        for (i = 0; i < 7; i++)
            pstNewFaceDetectAlarm->timeSpan.workday[i] = 0;
    }
    else
    {
        pstNewFaceDetectAlarm->arming_flag = 4;
        for (i = 0; i < Num_int; i++) 
        {
            cJSON *Days_i_pri = cJSON_GetArrayItem(pDays, i);
            alarm_day_plan_set(Days_i_pri, &pstNewFaceDetectAlarm->timeSpan);
        }
    }


    {
        FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stFaceAlarmArray[iCameraIdx], pstNewFaceDetectAlarm, sizeof(FaceDetectAlarm));
        }
        anj_config_alarm_fd_set(stFaceAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewFaceDetectAlarm)
    {
        anj_mw_free(pstNewFaceDetectAlarm);
        pstNewFaceDetectAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv intrusion detect set *************************/

int smart_intrusion_area_set(cJSON *pRECTAreaInfo, int flag)
{
    __DBG("smart intrusion area set\n");

    int i = 0;
    VideoRegionAiAlarm *pstNewRegionAlarm = (VideoRegionAiAlarm *) anj_mw_malloc(sizeof(VideoRegionAiAlarm));
    if (pstNewRegionAlarm == NULL)
    {
        __ERR("pstNewRegionAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewRegionAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    //cJSON *ID              = cJSON_GetObjectItem(pRECTAreaInfo, "ID");
    cJSON *pEnabled          = cJSON_GetObjectItem(pRECTAreaInfo, "Enabled");
    cJSON *pSensitivity      = cJSON_GetObjectItem(pRECTAreaInfo, "Sensitivity");

    cJSON *pPointNum         = cJSON_GetObjectItem(pRECTAreaInfo, "PointNum");
    cJSON *pPointList        = cJSON_GetObjectItem(pRECTAreaInfo, "PointList");
    cJSON *pNum              = cJSON_GetObjectItem(pRECTAreaInfo, "Num");
    cJSON *pDetectTargetList = cJSON_GetObjectItem(pRECTAreaInfo, "DetectTargetList");

    if (flag == 0)
    {
        cJSON *TimeThreshold = cJSON_GetObjectItem(pRECTAreaInfo, "TimeThreshold");
        if (TimeThreshold != NULL)
        {
            int TimeThreshold_int = TimeThreshold->valueint;
            pstNewRegionAlarm->data[flag].stayseconds = TimeThreshold_int;
        }
    }
    if (pEnabled != NULL)
    {
        int Enabled_int = pEnabled->valueint;
        if (Enabled_int == 1) 
        {
            pstNewRegionAlarm->data[flag].enable = 1;
            pstNewRegionAlarm->arming_flag = ARMING_ALLDAY;
        }
        else
        {
            pstNewRegionAlarm->data[flag].enable = 0;
            pstNewRegionAlarm->arming_flag = ARMING_DISABLE;
        }
    }
    if (pSensitivity != NULL)
    {
        int Sensitivity_int = pSensitivity->valueint;
        pstNewRegionAlarm->data[flag].sensitivity = Sensitivity_int;
    }
    if (pPointNum != NULL)
    {
        int PointNum_int = pPointNum->valueint;
        pstNewRegionAlarm->polygonArea.count = PointNum_int;
        for(i = 0; i < PointNum_int; i++)
        {
            cJSON *pXY = cJSON_GetArrayItem(pPointList, i);
            pstNewRegionAlarm->polygonArea.points[i].x = (cJSON_GetObjectItem(pXY, "X")->valueint) / 100;
            pstNewRegionAlarm->polygonArea.points[i].y = (cJSON_GetObjectItem(pXY, "Y")->valueint) / 100;
        }
    }

    if (pNum != NULL)
    {
        int Num_int = pNum->valueint;
        if (Num_int > 3)
            Num_int = 3;
        if (Num_int < 0)
            Num_int = 0;

        for(i = 0; i < Num_int; i++)
        {
            cJSON *pDetectTargetList_i = cJSON_GetArrayItem(pDetectTargetList, i);
            int Enabled_i               = cJSON_GetObjectItem(pDetectTargetList_i, "Enabled")->valueint;
            int Type_i                  = cJSON_GetObjectItem(pDetectTargetList_i, "Type")->valueint;

            int type = pstNewRegionAlarm->data[flag].type_filter;
            char type_buf[33] = {0};
            type_buf[32] = '\0';
            ToBin(type, type_buf);

            if (Enabled_i == 1)
            {
                switch(Type_i)
                {
                    case 2:
                        type_buf[31-4] = 0x31;
                    break;
                    case 1:
                        type_buf[31-6] = 0x31;
                        type_buf[31-3] = 0x31;
                        type_buf[31-2] = 0x31;
                        type_buf[31-1] = 0x31;
                    break;
                    case 0:
                        type_buf[31-0] = 0x31;
                    break;
                }
            }
            else
            {
                switch(Type_i)
                {
                    case 2:
                        type_buf[31-4] = 0x30;
                    break;
                    case 1:
                        type_buf[31-6] = 0x30;
                        type_buf[31-3] = 0x30;
                        type_buf[31-2] = 0x30;
                        type_buf[31-1] = 0x30;
                    break;
                    case 0:
                        type_buf[31-0] = 0x30;
                    break;
                }
            }

            type = ToInt(type_buf);
            pstNewRegionAlarm->data[flag].type_filter = type;
        }
    }

    pstNewRegionAlarm->data[flag].mode = flag;
    __DBG("intrusion_rect_area0_set over\n");

    {
        VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRegionAlarmArray[iCameraIdx], pstNewRegionAlarm, sizeof(VideoRegionAiAlarm));
        }

        anj_config_alarm_region_set(stRegionAlarmArray);
    }

    if (pstNewRegionAlarm != NULL)
    {
        anj_mw_free(pstNewRegionAlarm);
        pstNewRegionAlarm = NULL;
    }

    return 0;
}

int unv_smart_intrusion_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int flag)
{
    __DBG("unv smart intrusion areas set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pAreaJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pAreaJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *pRECTAreas    = (cJSON_GetObjectItem(pAreaJson, "PolygonInfoList"));
    cJSON *pNum          = (cJSON_GetObjectItem(pAreaJson, "Num"));
    if (pNum != NULL)
    {
        int Num_int = pNum->valueint;
        if(Num_int >= 1) 
        {
            cJSON *pArealist =cJSON_GetArrayItem(pRECTAreas, 0);
            smart_intrusion_area_set(pArealist, flag); 
        }
        else
        {
            status = HTTP_RES_STATUS_NOT_MODIFY;
            snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
            goto __EXIT;
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAreaJson != NULL)
    {
        cJSON_Delete(pAreaJson);
        pAreaJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_intrusion_linkage_actions_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart intrusion link set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoRegionAiAlarm *pstRegionAlarm = (VideoRegionAiAlarm *)anj_mw_malloc(sizeof(VideoRegionAiAlarm));
    if (pstRegionAlarm == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pLinkageActionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pLinkageActionJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int Num_action = cJSON_GetObjectItem(pLinkageActionJson, "Num")->valueint;
    if (Num_action < 1)
    {
        status = HTTP_RES_STATUS_OK;
        snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");
        goto __EXIT;
    }
    else
    {
        cJSON *pActions = cJSON_GetObjectItem(pLinkageActionJson, "Actions");
        int i = 0, j = 0;

        int iCameraIdx = 0;
        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        memcpy(pstRegionAlarm, &pstAlarmConfig->aiAlarm.regionAiAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

        MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
        VideoCaptureCfg stVideoCaptureCfg = {0};
        AudioCapture stAudioCaptureCfg = {0};
        memcpy(&stVideoCaptureCfg, &pMediaCfg->videoConfig[iCameraIdx].videoCapture, sizeof(VideoCaptureCfg));
        memcpy(&stAudioCaptureCfg, &pMediaCfg->audioConfig.audioCapture, sizeof(AudioCapture));

        pstRegionAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_DISABLE;        // 无使能没有节点
        pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;      // 无使能没有节点

        AudioFileList *pstAudioList = http_audio_file_list_get();

        for (i = 0; i < Num_action; i++)
        {
            cJSON *pLinkageActionInfo = cJSON_GetArrayItem(pActions, i);
            int ActID = cJSON_GetObjectItem(pLinkageActionInfo, "ActID")->valueint;

            __DBG("ActID:%d\n", ActID);
            if(ActID == 24)
            {
                int Enabled_int, WarnCount_int, AudioFileID_int, AudioVolume_int;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)//原本状态不刷掉
                        pstRegionAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_ALLDAY;
                    else
                        pstRegionAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_DISABLE;
                }

                cJSON *pActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (pActParam != NULL)
                {
                    cJSON *WarnCount = cJSON_GetObjectItem(pActParam, "WarnCount");
                    cJSON *AudioFileID = cJSON_GetObjectItem(pActParam, "AudioFileID");
                    cJSON *AudioVolume = cJSON_GetObjectItem(pActParam, "AudioVolume");
                    if (WarnCount != NULL)
                    {
                        WarnCount_int = WarnCount->valueint; 
                        pstRegionAlarm->alarmAction.audioAction.times = WarnCount_int;
                    }
                    if (AudioFileID != NULL)
                    {
                        AudioFileID_int = AudioFileID->valueint;
                        if (AudioFileID_int < 10)
                        {
                            memset(&(pstRegionAlarm->alarmAction.audioAction.filename), 0, sizeof(pstRegionAlarm->alarmAction.audioAction.filename));
                            strcpy(pstRegionAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                        }
                    }
                    if (AudioVolume != NULL)
                    {
                        AudioVolume_int = AudioVolume->valueint;
                        float vol_val = (float)AudioVolume_int;
                        vol_val = vol_val / 255 * 100;
                        stAudioCaptureCfg.volume_play = (int)vol_val;
                    }
                }
            }
            else if(ActID == 25)
            {
                int Enabled_int, Luminance_int;//, Interval_int
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)
                        pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_ALLDAY;
                    else
                        pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
                }

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    //cJSON *Interval = cJSON_GetObjectItem(ActParam, "Interval");
                    cJSON *Luminance = cJSON_GetObjectItem(ActParam, "Luminance");
                    if (Luminance != NULL)
                    {
                        Luminance_int = Luminance->valueint;
                        stVideoCaptureCfg.led_brightness_value = Luminance_int / 10;
                    }
                }
            }
            else if(ActID == 28)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstRegionAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                }

                __DBG("region alarm action enable_flag:%d, timespan_num:%d\n", 
                    pstRegionAlarm->alarmAction.audioAction.enable.enable_flag,
                    pstRegionAlarm->alarmAction.audioAction.enable.timespan_num);

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *pAudioCustomInfo = cJSON_GetObjectItem(ActParam, "AudioCustomInfo");
                    if (pAudioCustomInfo != NULL)
                    {
                        cJSON *pDays = cJSON_GetObjectItem(pAudioCustomInfo, "Days");
                        if (pDays != NULL)
                        {
                            cJSON *pDays_1 = cJSON_GetArrayItem(pDays, 0);//获取周一
                            if (pDays_1 != NULL)
                            {
                                cJSON *pTimeSectionInfos = cJSON_GetObjectItem(pDays_1, "TimeSectionInfos");
                                if (pTimeSectionInfos != NULL)
                                {
                                    cJSON *pNum = cJSON_GetObjectItem(pDays_1, "Num");
                                    if (pNum != NULL)
                                    {
                                        int Num_int = pNum->valueint;
                                        if (Num_int == 4 && Enabled_int != 0)
                                        {
                                            pstRegionAlarm->alarmAction.audioAction.enable.enable_flag = 4;
                                            int time_num = 0;

                                            for (j = 0; j < 4; j++)
                                            {
                                                cJSON *pTimeSectionInfos_i = cJSON_GetArrayItem(pTimeSectionInfos, j);
                                                if (pTimeSectionInfos_i != NULL)
                                                {
                                                    cJSON *Begin = cJSON_GetObjectItem(pTimeSectionInfos_i, "Begin");
                                                    cJSON *End = cJSON_GetObjectItem(pTimeSectionInfos_i, "End");
                                                    cJSON *AudioActParamInfo = cJSON_GetObjectItem(pTimeSectionInfos_i, "AudioActParamInfo");
                                                    if (AudioActParamInfo != NULL && j == 0)
                                                    {
                                                        cJSON *WarnCount = cJSON_GetObjectItem(AudioActParamInfo, "WarnCount");
                                                        cJSON *AudioFileID = cJSON_GetObjectItem(AudioActParamInfo, "AudioFileID");
                                                        if (WarnCount != NULL)
                                                        {
                                                            int WarnCount_int = WarnCount->valueint;
                                                            if (WarnCount_int > 3)
                                                                WarnCount_int = 3;
                                                            else if(WarnCount_int <= 0)
                                                                WarnCount_int = 1;

                                                            pstRegionAlarm->alarmAction.audioAction.times = WarnCount_int;
                                                        }

                                                        if (AudioFileID != NULL)
                                                        {
                                                            int AudioFileID_int = AudioFileID->valueint;
                                                            if (AudioFileID_int < 10)
                                                            {
                                                                memset(&(pstRegionAlarm->alarmAction.audioAction.filename), 0, sizeof(pstRegionAlarm->alarmAction.audioAction.filename));
                                                                strcpy(pstRegionAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                                                            }
                                                            __DBG("AudioFileID_int:%d, region alarm action filename:%s\n", AudioFileID_int, pstRegionAlarm->alarmAction.audioAction.filename);
                                                        }
                                                    }

                                                    if (Begin != NULL && End != NULL)
                                                    {
                                                        char *BeginStr = Begin->valuestring;
                                                        char *EndStr = End->valuestring;
                                                        __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                        int bhour, bminute, bsec, ehour, eminute, esec;
                                                        int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                        int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                        if (ret1 == 3 && ret2 == 3)
                                                        {
                                                            __DBG("Begin time: %d:%d:%d, End time: %d:%d:%d\n", bhour, bminute, ehour, bhour, eminute, esec);
                                                            //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                            {
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.hour = bhour;
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.minute = bminute;
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.sec = bsec;
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.hour = ehour;
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.minute = eminute;
                                                                pstRegionAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.sec = esec;

                                                                time_num ++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            __DBG("time_num:%d\n", time_num);
                                            pstRegionAlarm->alarmAction.audioAction.enable.timespan_num = time_num;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if(ActID == 29)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag = 0;
                }

                __DBG("region alarm action light_twinkle enable_flag:%d\n", pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag);
                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *LightCustomInfo = cJSON_GetObjectItem(ActParam, "LightCustomInfo");
                    if (LightCustomInfo != NULL)
                    {
                        cJSON *LightWeekPlanInfo = cJSON_GetObjectItem(LightCustomInfo, "LightWeekPlanInfo");
                        if (LightWeekPlanInfo != NULL)
                        {
                            cJSON *Days = cJSON_GetObjectItem(LightWeekPlanInfo, "Days");
                            if (Days != NULL)
                            {
                                cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                                if (Days_1 != NULL)
                                {
                                    cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                    if (TimeSectionInfos != NULL)
                                    {
                                        cJSON * Num = cJSON_GetObjectItem(Days_1, "Num");
                                        if (Num != NULL)
                                        {
                                            int Num_int = Num->valueint;
                                            if (Num_int == 4 && Enabled_int != 0)
                                            {
                                                pstRegionAlarm->alarmAction.light_twinkle_enable.enable_flag = 4;
                                                int time_num = 0;
                                                for (j = 0; j < 4; j++)
                                                {
                                                    cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                    if (TimeSectionInfos_i != NULL)
                                                    {
                                                        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                        cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                        if (Begin != NULL && End != NULL)
                                                        {
                                                            char *BeginStr = Begin->valuestring;
                                                            char *EndStr = End->valuestring;
                                                            __DBG("BeginStr:%s, EndStr:%s", BeginStr, EndStr);
                                                            int bhour, bminute, bsec, ehour, eminute, esec;
                                                            int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                            int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                            if (ret1 == 3 && ret2 == 3)
                                                            {
                                                                __DBG("Begin time: %d:%d:%d, End time: %d:%d:%d\n", bhour, bminute, ehour, bhour, eminute, esec);
                                                                //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                                {
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.hour = bhour;
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.minute = bminute;
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.sec = bsec;
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.hour = ehour;
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.minute = eminute;
                                                                    pstRegionAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.sec = esec;
                                                                    time_num ++;
                                                                }
                                                            }

                                                        }
                                                    }
                                                }
                                                __DBG("time_num:%d\n", time_num);
                                                pstRegionAlarm->alarmAction.light_twinkle_enable.timespan_num = time_num;

                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        {
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                anj_config_video_capture_set(&stVideoCaptureCfg, iCameraIdx);
            }
        }

        {
            anj_config_audio_capture_set(&stAudioCaptureCfg);
        }

        {
            VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stRegionAlarmArray[iCameraIdx], pstRegionAlarm, sizeof(VideoRegionAiAlarm));
            }
        
            anj_config_alarm_region_set(stRegionAlarmArray);
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstRegionAlarm != NULL)
    {
        anj_mw_free(pstRegionAlarm);
        pstRegionAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pLinkageActionJson != NULL)
    {
        cJSON_Delete(pLinkageActionJson);
        pLinkageActionJson = NULL;
    } 

    return HTTP_PUT_OK;
}

int unv_smart_intrusion_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int flag)//使能智能侦测，关闭移动侦测
{
    __DBG("unv smart intrusion rule set, flag:%d\n", flag);
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoRegionAiAlarm *pstNewRegionAlarm = (VideoRegionAiAlarm *) anj_mw_malloc(sizeof(VideoRegionAiAlarm));
    if (pstNewRegionAlarm == NULL)
    {
        __ERR("pstNewRegionAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewRegionAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1

    cJSON *pRulesJson = cJSON_Parse(pMsgBody); //free 2
    if(Response == NULL || pRulesJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    char type_buf[33] = {0};
    type_buf[32] = '\0';

    ToBin(pstNewRegionAlarm->data[flag].type_filter, type_buf);
    cJSON *Enabled = cJSON_GetObjectItem(pRulesJson, "Enabled");
    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1)
        {
            pstNewRegionAlarm->enable = 1;
            pstNewRegionAlarm->data[flag].enable = 1;
            pstNewRegionAlarm->arming_flag = ARMING_ALLDAY;
            pstNewRegionAlarm->data[flag].sensitivity = 6;
        }
        else
        {
            pstNewRegionAlarm->enable = 0;
            pstNewRegionAlarm->data[flag].enable = 0;
            pstNewRegionAlarm->arming_flag = ARMING_DISABLE;
        }
    }

    {
        VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRegionAlarmArray[iCameraIdx], pstNewRegionAlarm, sizeof(VideoRegionAiAlarm));
        }

        anj_config_alarm_region_set(stRegionAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewRegionAlarm != NULL)
    {
        anj_mw_free(pstNewRegionAlarm);
        pstNewRegionAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRulesJson != NULL)
    {
        cJSON_Delete(pRulesJson);
        pRulesJson = NULL;
    }

    return HTTP_PUT_OK;
}


int unv_smart_intrusion_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart intrusion weekplan set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoRegionAiAlarm *pstNewRegionAlarm = (VideoRegionAiAlarm *) anj_mw_malloc(sizeof(VideoRegionAiAlarm));
    if (pstNewRegionAlarm == NULL)
    {
        __ERR("pstNewRegionAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewRegionAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); // free 2
    if(Response == NULL || pWeekPlanJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int i;
    int Num_int;
    int Enabled_int;

    cJSON *Enabled = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    cJSON *Num = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    cJSON *Days = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    Enabled_int = Enabled->valueint;
    Num_int = Num->valueint;
    __DBG("enable:%d, num:%d\n", Enabled_int, Num_int);

    if (Enabled_int == 0) 
    {
        pstNewRegionAlarm->arming_flag = 0;
        for (i = 0; i < 7; i++)
            pstNewRegionAlarm->timeSpan.workday[i] = 0;
    }
    else
    {
        pstNewRegionAlarm->arming_flag = 4;
        for (i = 0; i < Num_int; i++) 
        {
            cJSON *Days_i = cJSON_GetArrayItem(Days, i);// free 6
            alarm_day_plan_set(Days_i, &pstNewRegionAlarm->timeSpan);
        }
    }

    {
        VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRegionAlarmArray[iCameraIdx], pstNewRegionAlarm, sizeof(VideoRegionAiAlarm));
        }

        anj_config_alarm_region_set(stRegionAlarmArray);
    }


    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewRegionAlarm != NULL)
    {
        anj_mw_free(pstNewRegionAlarm);
        pstNewRegionAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv video gate set *************************/

int smart_crossline_area_set(cJSON *pLineInfo, int i_flag, VideoGateAlarm *pstAlarm)
{
    __DBG("smart crossline area set:%d\n", i_flag);
    if(pLineInfo == NULL)
    {
        return -1;
    }

    int i = 0;
    cJSON *Enabled = cJSON_GetObjectItem(pLineInfo, "Enabled");
    cJSON *Sensitivity = cJSON_GetObjectItem(pLineInfo, "Sensitivity");
    cJSON *Direction = cJSON_GetObjectItem(pLineInfo, "Direction");
    cJSON *StartPoint = cJSON_GetObjectItem(pLineInfo, "StartPoint");
    cJSON *EndPoint = cJSON_GetObjectItem(pLineInfo, "EndPoint");

    cJSON *Num = cJSON_GetObjectItem(pLineInfo, "Num");
    cJSON *DetectTargetList = cJSON_GetObjectItem(pLineInfo, "DetectTargetList");
    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1)
            pstAlarm->data[i_flag].enable = 1;
        else
            pstAlarm->data[i_flag].enable = 0;
    }

    if (Sensitivity != NULL)
    {
        int Sensitivity_int = Sensitivity->valueint;
        pstAlarm->data[i_flag].sensitivity = Sensitivity_int;
    }

    if (Direction != NULL)
    {
        int Direction_int = Direction->valueint;
        if (Direction_int == 0)
            pstAlarm->data[i_flag].direction = 0;
        else if (Direction_int == 2)
            pstAlarm->data[i_flag].direction = 1;
        else if (Direction_int == 1)
            pstAlarm->data[i_flag].direction = 2;
    }
    if (StartPoint != NULL)
    {
        cJSON *X = cJSON_GetObjectItem(StartPoint, "X");
        cJSON *Y = cJSON_GetObjectItem(StartPoint, "Y");
        if (X != NULL)
        {
            int X_int = X->valueint;
            pstAlarm->data[i_flag].x0Pos = X_int / 100;
        }
        if (Y != NULL)
        {
            int Y_int = Y->valueint;
            pstAlarm->data[i_flag].y0Pos = Y_int / 100;
        }
    }
    if (EndPoint != NULL)
    {
        cJSON *X = cJSON_GetObjectItem(EndPoint, "X");
        cJSON *Y = cJSON_GetObjectItem(EndPoint, "Y");
        if (X != NULL)
        {
            int X_int = X->valueint;
            pstAlarm->data[i_flag].x1Pos = X_int / 100;
        }
        if (Y != NULL)
        {
            int Y_int = Y->valueint;
            pstAlarm->data[i_flag].y1Pos = Y_int / 100;
        }
    }

    if (Num != NULL)
    {
        int Num_int = Num->valueint;
        for(i = 0; i < Num_int; i++)
        {
            cJSON *DetectTargetList_i = cJSON_GetArrayItem(DetectTargetList, i);
            int Enabled_i = cJSON_GetObjectItem(DetectTargetList_i, "Enabled")->valueint;
            int Type_i = cJSON_GetObjectItem(DetectTargetList_i, "Type")->valueint;

            int type = pstAlarm->data[i_flag].type;
            char type_buf[33] = {0};
            type_buf[32] = '\0';
            ToBin(type, type_buf);

            if (Enabled_i == 1)
            {
                switch(Type_i)
                {
                    case 2:
                        type_buf[31-4] = 0x31;
                    break;
                    case 1:
                        type_buf[31-6] = 0x31;
                        type_buf[31-3] = 0x31;
                        type_buf[31-2] = 0x31;
                        type_buf[31-1] = 0x31;
                    break;
                    case 0:
                        type_buf[31-0] = 0x31;
                    break;
                }
            }
            else 
            {
                switch(Type_i)
                {
                    case 2:
                        type_buf[31-4] = 0x30;
                    break;
                    case 1:
                        type_buf[31-6] = 0x30;
                        type_buf[31-3] = 0x30;
                        type_buf[31-2] = 0x30;
                        type_buf[31-1] = 0x30;
                    break;
                    case 0:
                        type_buf[31-0] = 0x30;
                    break;
                }
            }

            type = ToInt(type_buf);
            pstAlarm->data[i_flag].type = type;
        }
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart_crossline weekplan set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewRegionAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.vgAlarm[iCameraIdx], sizeof(VideoGateAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;

    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); // free 2
    if(Response == NULL || pWeekPlanJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int i;
    int Num_int;
    int Enabled_int;

    cJSON *Enabled = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    cJSON *Num = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    cJSON *Days = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    Enabled_int = Enabled->valueint;
    Num_int = Num->valueint;

    __DBG("enable:%d, num:%d\n", Enabled_int, Num_int);

    if (Enabled_int == 0) 
    {
        pstNewVideoGateAlarm->arming_flag = 0;
        for (i = 0; i < 7; i++)
            pstNewVideoGateAlarm->timeSpan.workday[i] = 0;
    }
    else
    {
        pstNewVideoGateAlarm->arming_flag = 4;
        for (i = 0; i < Num_int; i++) 
        {
            cJSON *Days_i = cJSON_GetArrayItem(Days, i);// free 6
            alarm_day_plan_set(Days_i, &pstNewVideoGateAlarm->timeSpan);
        }
    }

    {
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
        }

        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_linkage_action_bright_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart crossline link bright set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewRegionAlarm malloc failed\n");
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pLinkageActionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pLinkageActionJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int Num_action = cJSON_GetObjectItem(pLinkageActionJson, "Num")->valueint;
    if (Num_action < 1)
    {
        goto __EXIT;
    }
    else
    {
        cJSON *Actions = cJSON_GetObjectItem(pLinkageActionJson, "Actions");
        int i = 0, j = 0;
        int iCameraIdx = 0;

        MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
        VideoCaptureCfg stNewVideoCapConfig = {0};
        AudioCapture stNewAudioCapConfig = {0};
        memcpy(&stNewVideoCapConfig, &pMediaCfg->videoConfig[iCameraIdx].videoCapture, sizeof(VideoCaptureCfg));
        memcpy(&stNewAudioCapConfig, &pMediaCfg->audioConfig.audioCapture, sizeof(AudioCapture));

        AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
        memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.vgAlarm[iCameraIdx], sizeof(VideoGateAlarm));

        AudioFileList *pstAudioList = http_audio_file_list_get();

        for (i = 0; i < Num_action; i++)
        {
            cJSON *pLinkageActionInfo = cJSON_GetArrayItem(Actions, i);
            int ActID = cJSON_GetObjectItem(pLinkageActionInfo, "ActID")->valueint;
            __DBG("ActID:%d\n", ActID);

            if(ActID == 24)
            {
                int Enabled_int, WarnCount_int, AudioFileID_int, AudioVolume_int;

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    cJSON *Enabled = cJSON_GetObjectItem(ActParam, "Enabled");
                    cJSON *WarnCount = cJSON_GetObjectItem(ActParam, "WarnCount");
                    cJSON *AudioFileID = cJSON_GetObjectItem(ActParam, "AudioFileID");
                    cJSON *AudioVolume = cJSON_GetObjectItem(ActParam, "AudioVolume");

                    if (Enabled != NULL)
                    {
                        Enabled_int = Enabled->valueint;
                        if (Enabled_int == 1)//原本状态不刷掉
                        {
                            if (pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_ALLDAY;
                        }
                        else
                        {
                            pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                        }
                    }

                    if (WarnCount != NULL)
                    {
                        WarnCount_int = WarnCount->valueint; 
                        pstNewVideoGateAlarm->alarmAction.audioAction.times = WarnCount_int;
                    }

                    if (AudioFileID != NULL)
                    {
                        AudioFileID_int = AudioFileID->valueint;
                        if (AudioFileID_int < 10)
                        {
                            memset(&(pstNewVideoGateAlarm->alarmAction.audioAction.filename), 0, AUDIO_ACTION_LEN_FILENAME);
                            strcpy(pstNewVideoGateAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                        }
                    }

                    if (AudioVolume != NULL)
                    {
                        AudioVolume_int = AudioVolume->valueint;
                        float vol_val = (float)AudioVolume_int;
                        vol_val = vol_val / 255 * 100;
                        stNewAudioCapConfig.volume_play = (int)vol_val;
                    }
                }
            }
            else if(ActID == 25)
            {
                int Enabled_int, Luminance_int;//, Interval_int

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    cJSON *Enabled = cJSON_GetObjectItem(ActParam, "Enabled");
                    //cJSON *Interval = cJSON_GetObjectItem(ActParam, "Interval");
                    cJSON *Luminance = cJSON_GetObjectItem(ActParam, "Luminance");

                    if (Enabled != NULL)
                    {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)
                    {    
                        if (pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag == ARMING_DISABLE)
                            pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag = ARMING_ALLDAY;
                        }
                        else
                        {
                            pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag = ARMING_DISABLE;
                        }
                    }

                    if (Luminance != NULL)
                    {
                        Luminance_int = Luminance->valueint;
                        stNewVideoCapConfig.led_brightness_value = Luminance_int / 10;
                    }
                }
            }
            else if(ActID == 28)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                }

                __DBG("video gate alarm action enable:%d, timespan_num:%d\n",
                    pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag,
                    pstNewVideoGateAlarm->alarmAction.audioAction.enable.timespan_num);

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *AudioCustomInfo = cJSON_GetObjectItem(ActParam, "AudioCustomInfo");
                    if (AudioCustomInfo != NULL)
                    {
                        cJSON *Days = cJSON_GetObjectItem(AudioCustomInfo, "Days");
                        if (Days != NULL)
                        {
                            cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                            if (Days_1 != NULL)
                            {
                                cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                if (TimeSectionInfos != NULL)
                                {
                                    cJSON *Num = cJSON_GetObjectItem(Days_1, "Num");
                                    if (Num != NULL)
                                    {
                                        int Num_int = Num->valueint;
                                        if (Num_int == 4 && Enabled_int != 0)
                                        {
                                            pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = 4;

                                            int time_num = 0;
                                            for (j = 0; j < 4; j++)
                                            {
                                                cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                if (TimeSectionInfos_i != NULL)
                                                {
                                                    cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                    cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                    cJSON *AudioActParamInfo = cJSON_GetObjectItem(TimeSectionInfos_i, "AudioActParamInfo");
                                                    if (AudioActParamInfo != NULL && j == 0)
                                                    {
                                                        cJSON *WarnCount = cJSON_GetObjectItem(AudioActParamInfo, "WarnCount");
                                                        cJSON *AudioFileID = cJSON_GetObjectItem(AudioActParamInfo, "AudioFileID");
                                                        if (WarnCount != NULL)
                                                        {
                                                            int WarnCount_int = WarnCount->valueint;
                                                            if (WarnCount_int > 3)
                                                                WarnCount_int = 3;
                                                            else if(WarnCount_int <= 0)
                                                                WarnCount_int = 1;

                                                            pstNewVideoGateAlarm->alarmAction.audioAction.times = WarnCount_int;
                                                        }

                                                        if (AudioFileID != NULL)
                                                        {
                                                            int AudioFileID_int = AudioFileID->valueint;
                                                            __DBG("AudioFileID_int:%d\n", AudioFileID_int);
                                                            if (AudioFileID_int < 10)
                                                            {
                                                                memset(&(pstNewVideoGateAlarm->alarmAction.audioAction.filename), 0, AUDIO_ACTION_LEN_FILENAME);
                                                                strcpy(pstNewVideoGateAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                                                            }

                                                            __DBG("video gate action filename:%s, AudioFileID:%d\n", pstNewVideoGateAlarm->alarmAction.audioAction.filename, AudioFileID_int);
                                                        }
                                                    }

                                                    if (Begin != NULL && End != NULL)
                                                    {
                                                        char *BeginStr = Begin->valuestring;
                                                        char *EndStr = End->valuestring;
                                                        __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                        int bhour, bminute, bsec, ehour, eminute, esec;
                                                        int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                        int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                        if (ret1 == 3 && ret2 == 3)
                                                        {
                                                            __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);
                                                            //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                            {
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.hour = bhour;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.minute = bminute;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.sec = bsec;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.hour = ehour;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.minute = eminute;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.sec = esec;

                                                                time_num ++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            __DBG("time_num:%d\n", time_num);
                                            pstNewVideoGateAlarm->alarmAction.audioAction.enable.timespan_num = time_num;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if(ActID == 29)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag = 0;
                }

                __DBG("video gate action light_twinkle enable:%d\n", pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag);

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *LightCustomInfo = cJSON_GetObjectItem(ActParam, "LightCustomInfo");
                    if (LightCustomInfo != NULL)
                    {
                        cJSON *LightWeekPlanInfo = cJSON_GetObjectItem(LightCustomInfo, "LightWeekPlanInfo");
                        if (LightWeekPlanInfo != NULL)
                        {
                            cJSON *Days = cJSON_GetObjectItem(LightWeekPlanInfo, "Days");
                            if (Days != NULL)
                            {
                                cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                                if (Days_1 != NULL)
                                {
                                    cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                    if (TimeSectionInfos != NULL)
                                    {
                                        cJSON * Num = cJSON_GetObjectItem(Days_1, "Num");
                                        if (Num != NULL)
                                        {
                                            int Num_int = Num->valueint;
                                            if (Num_int == 4 && Enabled_int != 0)
                                            {
                                                pstNewVideoGateAlarm->alarmAction.alarm_led_enable.enable_flag = 4;
                                                int time_num = 0;

                                                for (j = 0; j < 4; j++)
                                                {
                                                    cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                    if (TimeSectionInfos_i != NULL)
                                                    {
                                                        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                        cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                        if (Begin != NULL && End != NULL)
                                                        {
                                                            char *BeginStr = Begin->valuestring;
                                                            char *EndStr = End->valuestring;
                                                            __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                            int bhour, bminute, bsec, ehour, eminute, esec;
                                                            int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                            int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                            if (ret1 == 3 && ret2 == 3)
                                                            {
                                                                __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);
                                                                //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                                {
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].startTime.hour = bhour;
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].startTime.minute = bminute;
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].startTime.sec = bsec;
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].endTime.hour = ehour;
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].endTime.minute = eminute;
                                                                    pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timeSpans[j].endTime.sec = esec;
                                                                    time_num ++;
                                                                }
                                                            }

                                                        }
                                                    }
                                                }
                                                __DBG("time_num:%d\n", time_num);
                                                pstNewVideoGateAlarm->alarmAction.alarm_led_enable.timespan_num = time_num;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            anj_config_video_capture_set(&stNewVideoCapConfig, iCameraIdx);
        }

        anj_config_audio_capture_set(&stNewAudioCapConfig);

        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
        }
        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pLinkageActionJson != NULL)
    {
        cJSON_Delete(pLinkageActionJson);
        pLinkageActionJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_linkage_action_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart crossline link action set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewVideoGateAlarm malloc failed\n");
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pLinkageActionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pLinkageActionJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int Num_action = cJSON_GetObjectItem(pLinkageActionJson, "Num")->valueint;
    if (Num_action < 1)
    {
        goto __EXIT;
    }
    else
    {
        cJSON *Actions = cJSON_GetObjectItem(pLinkageActionJson, "Actions");
        int i = 0, j = 0;
        int iCameraIdx = 0;

        MediaConfig *pMediaCfg = (MediaConfig *)getMediaConfig();
        VideoCaptureCfg stVideoCapConfig = {0};
        AudioCapture stAudioCapConfig = {0};
        memcpy(&stVideoCapConfig, &pMediaCfg->videoConfig[iCameraIdx].videoCapture, sizeof(VideoCaptureCfg));
        memcpy(&stAudioCapConfig, &pMediaCfg->audioConfig.audioCapture, sizeof(AudioCapture));

        AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
        memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.vgAlarm[iCameraIdx], sizeof(VideoGateAlarm));

        pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_DISABLE;//无使能没有节点
        pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;//无使能没有节点

        AudioFileList *pstAudioList = http_audio_file_list_get();

        for (i = 0; i < Num_action; i++)
        {
            cJSON *pLinkageActionInfo = cJSON_GetArrayItem(Actions, i);
            int ActID = cJSON_GetObjectItem(pLinkageActionInfo, "ActID")->valueint;
            __DBG("ActID:%d\n", ActID);

            if(ActID == 24)
            {
                int Enabled_int, WarnCount_int, AudioFileID_int, AudioVolume_int;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)//原本状态不刷掉
                        pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_ALLDAY;
                    else
                        pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_DISABLE;
                }

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    cJSON *WarnCount = cJSON_GetObjectItem(ActParam, "WarnCount");
                    cJSON *AudioFileID = cJSON_GetObjectItem(ActParam, "AudioFileID");
                    cJSON *AudioVolume = cJSON_GetObjectItem(ActParam, "AudioVolume");
                    if (WarnCount != NULL)
                    {
                        WarnCount_int = WarnCount->valueint;
                        if (WarnCount_int > 3)
                            WarnCount_int = 3;
                        else if(WarnCount_int <= 0)
                            WarnCount_int = 1;
                        pstNewVideoGateAlarm->alarmAction.audioAction.times = WarnCount_int;
                    }

                    if (AudioFileID != NULL)
                    {
                        AudioFileID_int = AudioFileID->valueint;
                        if (AudioFileID_int < 10)
                        {
                            memset(&(pstNewVideoGateAlarm->alarmAction.audioAction.filename), 0, AUDIO_ACTION_LEN_FILENAME);
                            strcpy(pstNewVideoGateAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                        }
                    }

                    if (AudioVolume != NULL)
                    {
                        AudioVolume_int = AudioVolume->valueint;
                        float vol_val = (float)AudioVolume_int;
                        vol_val = vol_val / 255 * 100;
                        stAudioCapConfig.volume_play = (int)vol_val;
                    }
                }
            }
            else if(ActID == 25)
            {
                int Enabled_int, Luminance_int;//, Interval_int
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");

                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)
                        pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_ALLDAY;
                    else
                        pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
                }

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    //cJSON *Interval = cJSON_GetObjectItem(ActParam, "Interval");
                    cJSON *Luminance = cJSON_GetObjectItem(ActParam, "Luminance");

                    if (Luminance != NULL)
                    {
                        Luminance_int = Luminance->valueint;
                        stVideoCapConfig.led_brightness_value = Luminance_int / 10;
                    }
                }
            }
            else if(ActID == 28)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                }

                __DBG("video gate action enable:%d, timespan_num:%d\n", 
                    pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag, pstNewVideoGateAlarm->alarmAction.audioAction.enable.timespan_num);

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *AudioCustomInfo = cJSON_GetObjectItem(ActParam, "AudioCustomInfo");
                    if (AudioCustomInfo != NULL)
                    {
                        cJSON *Days = cJSON_GetObjectItem(AudioCustomInfo, "Days");
                        if (Days != NULL)
                        {
                            cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                            if (Days_1 != NULL)
                            {
                                cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                if (TimeSectionInfos != NULL)
                                {
                                    cJSON *Num = cJSON_GetObjectItem(Days_1, "Num");
                                    if (Num != NULL)
                                    {
                                        int Num_int = Num->valueint;
                                        if (Num_int == 4 && Enabled_int != 0)
                                        {
                                            pstNewVideoGateAlarm->alarmAction.audioAction.enable.enable_flag = 4;
                                            int time_num = 0;

                                            for (j = 0; j < 4; j++)
                                            {
                                                cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                if (TimeSectionInfos_i != NULL)
                                                {
                                                    cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                    cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                    cJSON *AudioActParamInfo = cJSON_GetObjectItem(TimeSectionInfos_i, "AudioActParamInfo");
                                                    if (AudioActParamInfo != NULL && j == 0)
                                                    {
                                                        cJSON *WarnCount = cJSON_GetObjectItem(AudioActParamInfo, "WarnCount");
                                                        cJSON *AudioFileID = cJSON_GetObjectItem(AudioActParamInfo, "AudioFileID");
                                                        if (WarnCount != NULL)
                                                        {
                                                            int WarnCount_int = WarnCount->valueint;
                                                            if (WarnCount_int > 3)
                                                                WarnCount_int = 3;
                                                            else if(WarnCount_int <= 0)
                                                                WarnCount_int = 1;

                                                            pstNewVideoGateAlarm->alarmAction.audioAction.times = WarnCount_int;
                                                        }

                                                        if (AudioFileID != NULL)
                                                        {
                                                            int AudioFileID_int = AudioFileID->valueint;
                                                            __DBG("AudioFileID_int:%d\n", AudioFileID_int);
                                                            if (AudioFileID_int < 10)
                                                            {
                                                                memset(&(pstNewVideoGateAlarm->alarmAction.audioAction.filename), 0, sizeof(pstNewVideoGateAlarm->alarmAction.audioAction.filename));
                                                                strcpy(pstNewVideoGateAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                                                            }

                                                            __DBG("video gate audio action filename:%s, AudioFileID:%d\n", pstNewVideoGateAlarm->alarmAction.audioAction.filename, AudioFileID_int);
                                                        }
                                                    }

                                                    if (Begin != NULL && End != NULL)
                                                    {
                                                        char *BeginStr = Begin->valuestring;
                                                        char *EndStr = End->valuestring;
                                                        __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                        int bhour, bminute, bsec, ehour, eminute, esec;
                                                        int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                        int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                        if (ret1 == 3 && ret2 == 3)
                                                        {
                                                            __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);

                                                            //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                            {
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.hour = bhour;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.minute = bminute;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.sec = bsec;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.hour = ehour;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.minute = eminute;
                                                                pstNewVideoGateAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.sec = esec;
                                                                time_num ++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            __DBG("time_num:%d\n", time_num);
                                           pstNewVideoGateAlarm->alarmAction.audioAction.enable.timespan_num = time_num;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if(ActID == 29)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(pLinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag = 0;
                }

                __DBG("video gate action light_twinkle enable:%d\n", pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag);

                cJSON *ActParam = cJSON_GetObjectItem(pLinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *LightCustomInfo = cJSON_GetObjectItem(ActParam, "LightCustomInfo");
                    if (LightCustomInfo != NULL)
                    {
                        cJSON *LightWeekPlanInfo = cJSON_GetObjectItem(LightCustomInfo, "LightWeekPlanInfo");
                        if (LightWeekPlanInfo != NULL)
                        {
                            cJSON *Days = cJSON_GetObjectItem(LightWeekPlanInfo, "Days");
                            if (Days != NULL)
                            {
                                cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                                if (Days_1 != NULL)
                                {
                                    cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                    if (TimeSectionInfos != NULL)
                                    {
                                        cJSON * Num = cJSON_GetObjectItem(Days_1, "Num");
                                        if (Num != NULL)
                                        {
                                            int Num_int = Num->valueint;
                                            if (Num_int == 4 && Enabled_int != 0)
                                            {
                                                pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.enable_flag = 4;
                                                int time_num = 0;
                                                for (j = 0; j < 4; j++)
                                                {
                                                    cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);

                                                    if (TimeSectionInfos_i != NULL)
                                                    {
                                                        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                        cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                        if (Begin != NULL && End != NULL)
                                                        {
                                                            char *BeginStr = Begin->valuestring;
                                                            char *EndStr = End->valuestring;
                                                            __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                            int bhour, bminute, bsec, ehour, eminute, esec;
                                                            int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                            int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                            if (ret1 == 3 && ret2 == 3)
                                                            {
                                                                __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);
                                                                //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                                {
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.hour = bhour;
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.minute = bminute;
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.sec = bsec;
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.hour = ehour;
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.minute = eminute;
                                                                    pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.sec = esec;
                                                                    time_num ++;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                __DBG("time_num:%d\n", time_num);
                                                pstNewVideoGateAlarm->alarmAction.light_twinkle_enable.timespan_num = time_num;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        {
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                anj_config_video_capture_set(&stVideoCapConfig, iCameraIdx);
            }
        }

        {
            anj_config_audio_capture_set(&stAudioCapConfig);
        }

        {
            VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
            }
        
            anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pLinkageActionJson != NULL)
    {
        cJSON_Delete(pLinkageActionJson);
        pLinkageActionJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_areas0_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int i_flag)//设置单个区域
{
    __DBG("unv smart crossline areas0 set:%d\n", i_flag);
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewVideoGateAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pRectAreaJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pRectAreaJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    if (i_flag < 0 || i_flag > 3)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");
        goto __EXIT;
    }

    smart_crossline_area_set(pRectAreaJson, i_flag, pstNewVideoGateAlarm);

    {
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
        }

        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRectAreaJson != NULL)
    {
        cJSON_Delete(pRectAreaJson);
        pRectAreaJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart crossline areas set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewVideoGateAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    int i = 0;
    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pRectAreaJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pRectAreaJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *Num = cJSON_GetObjectItem(pRectAreaJson, "Num");
    cJSON *LineInfoList = cJSON_GetObjectItem(pRectAreaJson, "LineInfoList");
    if (Num != NULL && LineInfoList != NULL)
    {
        int Num_int = Num->valueint;
        int LineInfoList_size = cJSON_GetArraySize(LineInfoList);
        if (Num_int == LineInfoList_size)
        {
            for (i = 0; i < Num_int; i++)
            {
                cJSON *LineInfoList_t = cJSON_GetArrayItem(LineInfoList, i);
                smart_crossline_area_set(LineInfoList_t, i, pstNewVideoGateAlarm);
            }
        }
    }

    {
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
        }

        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRectAreaJson != NULL)
    {
        cJSON_Delete(pRectAreaJson);
        pRectAreaJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_crossline_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)//使能智能侦测，关闭移动侦测
{
    __DBG("unv smart crossline rule set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    VideoGateAlarm *pstNewVideoGateAlarm = (VideoGateAlarm *) anj_mw_malloc(sizeof(VideoGateAlarm));
    if (pstNewVideoGateAlarm == NULL)
    {
        __ERR("pstNewVideoGateAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewVideoGateAlarm, &pAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pRulesJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pRulesJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *Enabled = cJSON_GetObjectItem(pRulesJson, "Enabled");
    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1)
            pstNewVideoGateAlarm->enable = 1;
        else
            pstNewVideoGateAlarm->enable = 0;
    }
    else
    {
        Enabled = cJSON_GetObjectItem(pRulesJson, "Enable");
        if (Enabled != NULL)
        {
            int Enabled_int = Enabled->valueint;
            if (Enabled_int == 1)
                pstNewVideoGateAlarm->enable = 1;
            else
                pstNewVideoGateAlarm->enable = 0;
        }
    }

    {
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], pstNewVideoGateAlarm, sizeof(VideoGateAlarm));
        }

        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewVideoGateAlarm != NULL)
    {
        anj_mw_free(pstNewVideoGateAlarm);
        pstNewVideoGateAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRulesJson != NULL)
    {
        cJSON_Delete(pRulesJson);
        pRulesJson = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv smart vehicle set *************************/

int pd_rect_area_set(const char *p)
{
    int i = 0;

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarm = {0};
    memcpy(&stPdAlarm, &pAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    cJSON *pAreaInfoJson = cJSON_Parse(p); //free 2
    //cJSON *ID = cJSON_GetObjectItem(pAreaInfoJson, "ID");
    cJSON *Enabled = cJSON_GetObjectItem(pAreaInfoJson, "Enabled");
    cJSON *Num = cJSON_GetObjectItem(pAreaInfoJson, "Num");
    cJSON *Sensitivity = cJSON_GetObjectItem(pAreaInfoJson, "Sensitivity");

    cJSON *PolygonInfoList = pAreaInfoJson;
    cJSON *RECTArea = cJSON_GetObjectItem(pAreaInfoJson, "RECTArea");
    cJSON *AreaInfoList = cJSON_GetObjectItem(pAreaInfoJson, "AreaInfoList");
    cJSON *DetectTargetList = cJSON_GetObjectItem(pAreaInfoJson, "DetectTargetList");

    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1) 
        {
            stPdAlarm.enable = 1;
            stPdAlarm.arming_flag = ARMING_ALLDAY;
        }
        else
        {
            stPdAlarm.enable = 0;
            stPdAlarm.arming_flag = ARMING_DISABLE;
        }
    }

    if (Sensitivity != NULL)
    {
        int sensitivity_int = Sensitivity->valueint;
        stPdAlarm.sensitivity = sensitivity_int/10; 
        stPdAlarm.threshold = sensitivity_int%10;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_CAR] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_MOTO] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_ELECTRICBICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_BICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_HUMAN] = sensitivity_int;
    }

    if (PolygonInfoList != NULL)
    {
        cJSON *PointList   = cJSON_GetObjectItem(PolygonInfoList, "PointList");
        cJSON *TopLeft     = cJSON_GetArrayItem(PointList, 0);
        cJSON *BottomRight = cJSON_GetArrayItem(PointList, 1);

        if (TopLeft != NULL && BottomRight != NULL)
        {
            int TX = cJSON_GetObjectItem(TopLeft, "X")->valueint;
            int TY = cJSON_GetObjectItem(TopLeft, "Y")->valueint;
            int BX = cJSON_GetObjectItem(BottomRight, "X")->valueint;
            int BY = cJSON_GetObjectItem(BottomRight, "Y")->valueint;
            TX = TX / 100;
            TY = TY / 100;
            BX = BX / 100;
            BY = BY / 100;

            stPdAlarm.polygonArea.count = 4;
            stPdAlarm.polygonArea.points[0].x = TX;
            stPdAlarm.polygonArea.points[0].y = TY;
            stPdAlarm.polygonArea.points[1].x = BX;
            stPdAlarm.polygonArea.points[1].y = TY;
            stPdAlarm.polygonArea.points[2].x = BX;
            stPdAlarm.polygonArea.points[2].y = BY;
            stPdAlarm.polygonArea.points[3].x = TX;
            stPdAlarm.polygonArea.points[3].y = BY ;
        }
    }

    if (AreaInfoList != NULL)
    {
        cJSON *PointNum  = cJSON_GetObjectItem(AreaInfoList, "PointNum");
        cJSON *PointList = cJSON_GetObjectItem(AreaInfoList, "PointList");
        if (PointNum != NULL && PointList != NULL)
        {
            int PointNum_int = PointNum->valueint;
            stPdAlarm.polygonArea.count = PointNum_int;
            for(i = 0; i < PointNum_int; i++)
            {
                cJSON *XY = cJSON_GetArrayItem(PointList, i);
                stPdAlarm.polygonArea.points[i].x = cJSON_GetObjectItem(XY, "X")->valueint;
                stPdAlarm.polygonArea.points[i].y = cJSON_GetObjectItem(XY, "Y")->valueint;
            }
        }
    }

    if (RECTArea != NULL)
    {
        cJSON *TopLeft     = cJSON_GetObjectItem(RECTArea, "TopLeft");
        cJSON *BottomRight = cJSON_GetObjectItem(RECTArea, "BottomRight");
        if (TopLeft != NULL && BottomRight != NULL)
        {
            int TX = cJSON_GetObjectItem(TopLeft, "X")->valueint;
            int TY = cJSON_GetObjectItem(TopLeft, "Y")->valueint;
            int BX = cJSON_GetObjectItem(BottomRight, "X")->valueint;
            int BY = cJSON_GetObjectItem(BottomRight, "Y")->valueint;
            stPdAlarm.area.xPos   = TX; 
            stPdAlarm.area.yPos   = TY;
            stPdAlarm.area.width  = BX - TX; 
            stPdAlarm.area.height = BY - TY;
        }
    }

    if (Num != NULL && DetectTargetList != NULL)
    {
        int Num_int = Num->valueint;
        for(i = 0; i < Num_int; i++)
        {
            cJSON *DetectTargetList_i = (cJSON_GetArrayItem(DetectTargetList, i));
            int Enabled_i             = cJSON_GetObjectItem(DetectTargetList_i, "Enabled")->valueint;
            int Type_i                = cJSON_GetObjectItem(DetectTargetList_i, "Type")->valueint;

            int type = stPdAlarm.type;
            char type_buf[33] = {0};
            type_buf[32] = '\0';
            ToBin(type, type_buf);

            if (Enabled_i)
            {
                switch(Type_i)
                {
                    case 0:
                        type_buf[31-4] = 0x31;
                    break;
                    case 1:
                        type_buf[31-3] = 0x31;
                        type_buf[31-1] = 0x31;
                    break;
                    case 2:
                        type_buf[31-0] = 0x31;
                    break;
                }
            }
            else 
            {
                switch(Type_i)
                {
                    case 0:
                        type_buf[31-4] = 0x30;
                    break;
                    case 1:
                        type_buf[31-3] = 0x30;
                        type_buf[31-1] = 0x30;
                    break;
                    case 2:
                        type_buf[31-0] = 0x30;
                    break;
                }
            }
            type = ToInt(type_buf);
            stPdAlarm.type = type;
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }
        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    if (pAreaInfoJson != NULL)
    {
        cJSON_Delete(pAreaInfoJson);
        pAreaInfoJson = NULL;
    }

    return 0;
}

int pd_polygon_info_set(const char *p)
{
    __DBG("PD polygon info set\n");

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarm = {0};
    memcpy(&stPdAlarm, &pAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    cJSON *pAreaJson = cJSON_Parse(p); //free 3
    if(pAreaJson == NULL)
    {
        goto __EXIT;
    }

    //cJSON *ID            = cJSON_GetObjectItem(MotionDetectionRECTAreaInfo, "ID");
    cJSON *Enabled      = cJSON_GetObjectItem(pAreaJson, "Enabled");
    cJSON *Sensitivity  = cJSON_GetObjectItem(pAreaJson, "Sensitivity");
    //cJSON *PointNum   = cJSON_GetObjectItem(MotionDetectionRECTAreaInfo, "PointNum");
    cJSON *PointList    = cJSON_GetObjectItem(pAreaJson, "PointList");
    cJSON *TopLeft      = cJSON_GetArrayItem(PointList, 0);
    cJSON *BottomRight  = cJSON_GetArrayItem(PointList, 1);

    if (0)//Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;

        //todo
        //if ((Enabled_int == 1) && (g_yen_version_car_clock == 1 || g_yen_version_pd_clock == 1))
        if (Enabled_int == 1)
        {
            stPdAlarm.enable        = 1;
            stPdAlarm.arming_flag   = ARMING_ALLDAY;
        }
        else
        {
            stPdAlarm.enable        = 0;
            stPdAlarm.arming_flag   = ARMING_ALLDAY;
        }
    }

    if (Sensitivity != NULL)
    {
        int sensitivity_int = Sensitivity->valueint;
        stPdAlarm.sensitivity    = sensitivity_int / 10;
        if (stPdAlarm.sensitivity > 10)
        {
            stPdAlarm.sensitivity = 10;
            stPdAlarm.threshold = 0 ;
        }
        else
        {
            stPdAlarm.threshold = sensitivity_int % 10;
        }

        stPdAlarm.sensitivitys[AI_TYPE_BIT_CAR] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_MOTO] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_ELECTRICBICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_BICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_HUMAN] = sensitivity_int;
    }

    if (TopLeft != NULL && BottomRight != NULL)
    {
        int TX = cJSON_GetObjectItem(TopLeft, "X")->valueint;
        int TY = cJSON_GetObjectItem(TopLeft, "Y")->valueint;
        int BX = cJSON_GetObjectItem(BottomRight, "X")->valueint;
        int BY = cJSON_GetObjectItem(BottomRight, "Y")->valueint;
        TX = TX / 100;
        TY = TY / 100;
        BX = BX / 100;
        BY = BY / 100;

        stPdAlarm.polygonArea.count = 4;
        stPdAlarm.polygonArea.points[0].x = TX;
        stPdAlarm.polygonArea.points[0].y = TY;
        stPdAlarm.polygonArea.points[1].x = BX;
        stPdAlarm.polygonArea.points[1].y = TY;
        stPdAlarm.polygonArea.points[2].x = BX;
        stPdAlarm.polygonArea.points[2].y = BY;
        stPdAlarm.polygonArea.points[3].x = TX;
        stPdAlarm.polygonArea.points[3].y = BY ;
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }
        anj_config_alarm_pd_set(stPdAlarmArray);
    }

__EXIT:
    if (pAreaJson != NULL)
    {
        cJSON_Delete(pAreaJson);
        pAreaJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_vehicle_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv pd rule set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    PdAlarm *pstNewPdAlarm = (PdAlarm *) anj_mw_malloc(sizeof(PdAlarm));
    if (pstNewPdAlarm == NULL)
    {
        __ERR("pstNewPdAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewPdAlarm, &pAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(sizeof(char)*ResponseLen); //free 1
    cJSON *pRuleJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pRuleJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    char type_buf[33] = {0};
    type_buf[32] = '\0';
    ToBin(pstNewPdAlarm->type, type_buf);
    __DBG("type_buf:%s\n", type_buf);

    cJSON *Enabled = cJSON_GetObjectItem(pRuleJson, "Enabled");
    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1)
        {
            pstNewPdAlarm->enable       = 1;
            pstNewPdAlarm->type         |= (1<<0);
            pstNewPdAlarm->sensitivity  = 6;

            //todo
            //g_yen_version_car_clock = 1;//宇视车型clock
        }
        else
        {
            pstNewPdAlarm->type &= (~(1<<0));
            //g_yen_version_car_clock = 0;//宇视车型clock
            //if (g_yen_version_car_clock == 0 && g_yen_version_pd_clock == 0)
            {
                pstNewPdAlarm->enable    = 0;
            }
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], pstNewPdAlarm, sizeof(PdAlarm));
        }
        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewPdAlarm != NULL)
    {
        anj_mw_free(pstNewPdAlarm);
        pstNewPdAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRuleJson != NULL)
    {
        cJSON_Delete(pRuleJson);
        pRuleJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_vehicle_areas0_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart vehicle areas0 set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;    
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 2
    cJSON *pAreaJson = cJSON_Parse(pMsgBody); //free 3

    if(Response == NULL || pAreaJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    pd_rect_area_set(pMsgBody);
    __DBG("unv smart vehicle area0 set succ\n");

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAreaJson != NULL)
    {
        cJSON_Delete(pAreaJson);
        pAreaJson = NULL;
    }

    return HTTP_PUT_OK;
}

//
int unv_smart_vehicle_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv smart vehicle areas set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    char *pRectInfoStr = NULL;

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen);
    cJSON *pAreaListJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pAreaListJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *PolygonInfoList = (cJSON_GetObjectItem(pAreaListJson, "PolygonInfoList"));
    cJSON *RECTAreas = (cJSON_GetObjectItem(pAreaListJson, "RECTAreas"));
    cJSON *Num = (cJSON_GetObjectItem(pAreaListJson, "Num"));

    if(Num != NULL && RECTAreas != NULL)
    {
        int Num_int = Num->valueint;
        if(Num_int == 1) 
        {
            cJSON *pRectAreaJson =(cJSON_GetArrayItem(RECTAreas, 0));
            pRectInfoStr = cJSON_Print(pRectAreaJson);
            pd_rect_area_set((const char *)pRectInfoStr); 
        }
        else
        {
            status = HTTP_RES_STATUS_NOT_MODIFY;
            snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
            goto __EXIT;
        }
    }
    else if(Num != NULL && PolygonInfoList != NULL)
    {
        int Num_int = Num->valueint;
        if(Num_int == 1) 
        {
            cJSON *pRectAreaJson =(cJSON_GetArrayItem(PolygonInfoList, 0));
            pRectInfoStr =  cJSON_Print(pRectAreaJson);//free 4
            pd_polygon_info_set(pRectInfoStr); 
        }
        else
        {
            status = HTTP_RES_STATUS_NOT_MODIFY;
            snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
            goto __EXIT;
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAreaListJson != NULL)
    {
        cJSON_Delete(pAreaListJson);
        pAreaListJson = NULL;
    }

    if (pRectInfoStr)
    {
        anj_mw_free(pRectInfoStr);
        pRectInfoStr = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv smart md set *************************/

int alarm_motion_area_set(char *p)
{
    __DBG("alarm motion area set\n");

    int i = 0;
    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarm = {0};
    memcpy(&stPdAlarm, &pAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    cJSON *pRectAreaJson = cJSON_Parse(p); //free 2
    //cJSON *ID = cJSON_GetObjectItem(pRectAreaJson, "ID");
    //cJSON *Enabled = cJSON_GetObjectItem(pRectAreaJson, "Enabled");
    cJSON *Sensitivity = cJSON_GetObjectItem(pRectAreaJson, "Sensitivity");

    cJSON *AreaInfoList = cJSON_GetObjectItem(pRectAreaJson, "AreaInfoList");
    cJSON *PointNum     = cJSON_GetObjectItem(AreaInfoList, "PointNum");
    cJSON *PointList    = cJSON_GetObjectItem(AreaInfoList, "PointList");
    cJSON *Num          = cJSON_GetObjectItem(pRectAreaJson, "Num");
    cJSON *DetectTargetList = cJSON_GetObjectItem(pRectAreaJson, "DetectTargetList");
    /*
    char *Enabled_pri = cJSON_Print(Enabled);//free 3
    HTTP_LOG("Enabled_pri:%s\n", Enabled_pri);
    if (!strcmp("1", Enabled_pri)) 
    {
        pAlarmt.enable    = 1;
        pAlarmt.arming_flag = ARMING_ALLDAY;
    }
    else
    {
        pAlarmt.enable    = 0;
        pAlarmt.arming_flag = ARMING_DISABLE;
    }
    */

    if (Sensitivity != NULL)
    {
        int sensitivity_int     = Sensitivity->valueint;
        stPdAlarm.sensitivity   = (sensitivity_int) / 10; 
        stPdAlarm.threshold     = (sensitivity_int) % 10;

        stPdAlarm.sensitivitys[AI_TYPE_BIT_CAR] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_MOTO] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_ELECTRICBICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_BICYCLE] = sensitivity_int;
        stPdAlarm.sensitivitys[AI_TYPE_BIT_HUMAN] = sensitivity_int;
    }

    if (PointNum != NULL)
    {
        int PointNum_int = PointNum->valueint;
        stPdAlarm.polygonArea.count = PointNum_int;

        for(i = 0; i < PointNum_int; i++)
        {
            cJSON *XY = cJSON_GetArrayItem(PointList, i);
            if (XY != NULL)
            {
                int X_int = cJSON_GetObjectItem(XY, "X")->valueint;
                int Y_int = cJSON_GetObjectItem(XY, "Y")->valueint;
                stPdAlarm.polygonArea.points[i].x = X_int / 100;
                stPdAlarm.polygonArea.points[i].y = Y_int / 100;
            }
        }
    }

    if (Num != NULL)
    {
        int Num_int = Num->valueint;
        for(i = 0; i < Num_int; i++)
        {
            cJSON *DetectTargetList_i = (cJSON_GetArrayItem(DetectTargetList, i));
            int Enable_int = cJSON_GetObjectItem(DetectTargetList_i, "Enabled")->valueint;
            int Type_i_int = cJSON_GetObjectItem(DetectTargetList_i, "Type")->valueint;

            int type = stPdAlarm.type;
            char type_buf[33] = {0};
            type_buf[32] = '\0';
            ToBin(type, type_buf);

            __DBG("enbale:%d, type:%d\n", Enable_int, Type_i_int);  //人2非1车0

            if (Enable_int)
            {
                switch(Type_i_int)//-4人,2车
                {
                    case 2:
                        type_buf[31-4] = 0x31;
                        //g_yen_version_pd_clock = 1;
                    break;
                    case 1:    //
                        type_buf[31-3] = 0x31;
                        type_buf[31-1] = 0x31;
                        //g_yen_version_car_clock = 1;
                    break;
                    case 0:    //
                        type_buf[31-0] = 0x31;
                    break;
                    default:
                    break;
                }
            }
            else 
            {
                switch(Type_i_int)
                {
                    case 2:
                        type_buf[31-4] = 0x30;
                        //g_yen_version_pd_clock = 0;
                    break;
                    case 1:    //
                        type_buf[31-3] = 0x30;
                        type_buf[31-1] = 0x30;
                        //g_yen_version_car_clock = 0;
                    break;
                    case 0:    //
                        type_buf[31-0] = 0x30;
                    break;
                    default :
                    break;
                }
            }

            type = ToInt(type_buf);
            stPdAlarm.type = type;
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }
        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    if (pRectAreaJson != NULL)
    {
        cJSON_Delete(pRectAreaJson);
        pRectAreaJson = NULL;
    }

    return 0;
}


int unv_alarm_motion_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)//使能智能侦测，关闭移动侦测
{
    __DBG("unv alarm motion rule set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    PdAlarm stPdAlarm = {0};
    MotionDetectAlarm stMotionAlarm = {0};

    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(&stPdAlarm, &pAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));
    memcpy(&stMotionAlarm, &pAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pRuleJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pRuleJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *Mode = cJSON_GetObjectItem(pRuleJson, "Mode");
    cJSON *Enabled = cJSON_GetObjectItem(pRuleJson, "Enabled");

    if (Mode != NULL && Enabled != NULL)
    {
        int Mode_int = Mode->valueint;
        int Enabled_int = Enabled->valueint;
        if (Mode_int == 1)
        {
            unv_motion_detect_mode_set(1);
            if (Enabled_int == 1)
            {
                stPdAlarm.enable = 1;
                stPdAlarm.arming_flag = ARMING_ALLDAY;

                stMotionAlarm.enable = 0;
                //g_yen_version_ultramotion_clock = 1;
            }
            else
            {
                stPdAlarm.enable = 0;
                stPdAlarm.arming_flag = ARMING_DISABLE;
                //g_yen_version_ultramotion_clock = 0;
            }
        }
        else
        {
            unv_motion_detect_mode_set(0);
            if (Enabled_int == 1)
            {
                stPdAlarm.enable = 0;

                stMotionAlarm.enable = 1;
                stMotionAlarm.arming_flag = ARMING_ALLDAY;
                //g_yen_version_ultramotion_clock = 0;
            }
            else
            {
                stMotionAlarm.enable = 1;
                stMotionAlarm.arming_flag = ARMING_DISABLE;
                //g_yen_version_ultramotion_clock = 1;
            }
        }
    }

    {
        MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stMotionAlarmArray[iCameraIdx], &stMotionAlarm, sizeof(MotionDetectAlarm));
        }
        anj_config_alarm_motion_set(stMotionAlarmArray);
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }
        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRuleJson != NULL)
    {
        cJSON_Delete(pRuleJson);
        pRuleJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_alarm_motion_linkage_action_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    PdAlarm *pstPdAlarm = (PdAlarm *)anj_mw_malloc(sizeof(PdAlarm));
    if (pstPdAlarm == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pLinkageActionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pLinkageActionJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int Num_action = cJSON_GetObjectItem(pLinkageActionJson, "Num")->valueint;
    if (Num_action < 1)
    {
        goto __EXIT;
    }
    else
    {
        cJSON *Actions = cJSON_GetObjectItem(pLinkageActionJson, "Actions");
        int i = 0, j = 0;

        int iCameraIdx = 0;
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        VideoCaptureCfg stVideoCapture = {0};
        AudioCapture stAudioCapture = {0};
        memcpy(&stVideoCapture, &pstMediaCfg->videoConfig[iCameraIdx].videoCapture, sizeof(VideoCaptureCfg));
        memcpy(&stAudioCapture, &pstMediaCfg->audioConfig.audioCapture, sizeof(AudioCapture));

        AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
        memcpy(pstPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

        AudioFileList *pstAudioList = http_audio_file_list_get();

        for (i = 0; i < Num_action; i++)
        {
            cJSON *LinkageActionInfo = cJSON_GetArrayItem(Actions, i);
            int ActID = cJSON_GetObjectItem(LinkageActionInfo, "ActID")->valueint;

            __DBG("unv motion link ActID:%d\n", ActID);
            if(ActID == 24)
            {
                int Enabled_int, WarnCount_int, AudioFileID_int, AudioVolume_int;

                cJSON *ActParam = cJSON_GetObjectItem(LinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    cJSON *Enabled = cJSON_GetObjectItem(ActParam, "Enabled");
                    cJSON *WarnCount = cJSON_GetObjectItem(ActParam, "WarnCount");
                    cJSON *AudioFileID = cJSON_GetObjectItem(ActParam, "AudioFileID");
                    cJSON *AudioVolume = cJSON_GetObjectItem(ActParam, "AudioVolume");

                    if (Enabled != NULL)
                    {
                        Enabled_int = Enabled->valueint;
                        if (Enabled_int == 1)//原本状态不刷掉
                        {
                            if (pstPdAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
                                pstPdAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_ALLDAY;
                        }
                        else
                            pstPdAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                    }

                    if (WarnCount != NULL)
                    {
                        WarnCount_int = WarnCount->valueint; 
                        pstPdAlarm->alarmAction.audioAction.times = WarnCount_int;
                    }

                    if (AudioFileID != NULL)
                    {
                        AudioFileID_int = AudioFileID->valueint;
                        if (AudioFileID_int < 10)
                        {
                            memset(&(pstPdAlarm->alarmAction.audioAction.filename), 0, AUDIO_ACTION_LEN_FILENAME);
                            strcpy(pstPdAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                        }
                    }

                    if (AudioVolume != NULL)
                    {
                        AudioVolume_int = AudioVolume->valueint;
                        float vol_val = (float)AudioVolume_int;
                        vol_val = vol_val / 255 * 100;
                        stAudioCapture.volume_play = (int)vol_val;
                    }
                }
            }
            else if(ActID == 25)
            {
                int Enabled_int, Luminance_int;//, Interval_int

                cJSON *ActParam = cJSON_GetObjectItem(LinkageActionInfo, "ActParam");
                if (ActParam != NULL)
                {
                    cJSON *Enabled = cJSON_GetObjectItem(ActParam, "Enabled");
                    //cJSON *Interval = cJSON_GetObjectItem(ActParam, "Interval");
                    cJSON *Luminance = cJSON_GetObjectItem(ActParam, "Luminance");

                    if (Enabled != NULL)
                    {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 1)
                    {    
                        if (pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
                            pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_ALLDAY;
                        }
                        else
                            pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_DISABLE;
                    }

                    if (Luminance != NULL)
                    {
                        Luminance_int = Luminance->valueint;
                        stVideoCapture.led_brightness_value = Luminance_int / 10;
                    }
                }
            }
            else if(ActID == 28)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(LinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstPdAlarm->alarmAction.audioAction.enable.enable_flag = 0;
                }

                __DBG("pd audio action enable:%d, timespan_num:%d\n", 
                    pstPdAlarm->alarmAction.audioAction.enable.enable_flag, 
                    pstPdAlarm->alarmAction.audioAction.enable.timespan_num);

                cJSON *ActParam = cJSON_GetObjectItem(LinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *AudioCustomInfo = cJSON_GetObjectItem(ActParam, "AudioCustomInfo");
                    if (AudioCustomInfo != NULL)
                    {
                        cJSON *Days = cJSON_GetObjectItem(AudioCustomInfo, "Days");
                        if (Days != NULL)
                        {
                            cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                            if (Days_1 != NULL)
                            {
                                cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                if (TimeSectionInfos != NULL)
                                {
                                    cJSON *Num = cJSON_GetObjectItem(Days_1, "Num");
                                    if (Num != NULL)
                                    {
                                        int Num_int = Num->valueint;
                                        if (Num_int == 4 && Enabled_int != 0)
                                        {
                                            pstPdAlarm->alarmAction.audioAction.enable.enable_flag = ARMING_CUSTOM;
                                            int time_num = 0;

                                            for (j = 0; j < 4; j++)
                                            {
                                                cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                if (TimeSectionInfos_i != NULL)
                                                {
                                                    cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                    cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                    cJSON *AudioActParamInfo = cJSON_GetObjectItem(TimeSectionInfos_i, "AudioActParamInfo");
                                                    if (AudioActParamInfo != NULL && j == 0)
                                                    {
                                                        cJSON *WarnCount = cJSON_GetObjectItem(AudioActParamInfo, "WarnCount");
                                                        cJSON *AudioFileID = cJSON_GetObjectItem(AudioActParamInfo, "AudioFileID");
                                                        if (WarnCount != NULL)
                                                        {
                                                            int WarnCount_int = WarnCount->valueint;
                                                            if (WarnCount_int > 3)
                                                                WarnCount_int = 3;
                                                            else if(WarnCount_int <= 0)
                                                                WarnCount_int = 1;

                                                            pstPdAlarm->alarmAction.audioAction.times = WarnCount_int;
                                                        }

                                                        if (AudioFileID != NULL)
                                                        {
                                                            int AudioFileID_int = AudioFileID->valueint;
                                                            if (AudioFileID_int < 10)
                                                            {
                                                                memset(&(pstPdAlarm->alarmAction.audioAction.filename), 0, sizeof(pstPdAlarm->alarmAction.audioAction.filename));
                                                                strcpy(pstPdAlarm->alarmAction.audioAction.filename, pstAudioList->Item[AudioFileID_int].file_pathname);
                                                            }
                                                        }
                                                    }

                                                    if (Begin != NULL && End != NULL)
                                                    {
                                                        char *BeginStr = Begin->valuestring;
                                                        char *EndSrt = End->valuestring;
                                                        __DBG("Begin:%s. EndStr:%s\n", BeginStr, EndSrt);

                                                        int bhour, bminute, bsec, ehour, eminute, esec;
                                                        int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                        int ret2 = sscanf(EndSrt, "%d:%d:%d", &ehour, &eminute, &esec);
                                                        if (ret1 == 3 && ret2 == 3)
                                                        {
                                                            __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);
                                                            //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                            {
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.hour = bhour;
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.minute = bminute;
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].startTime.sec = bsec;
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.hour = ehour;
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.minute = eminute;
                                                                pstPdAlarm->alarmAction.audioAction.enable.timeSpans[j].endTime.sec = esec;
                                                                time_num ++;
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                            pstPdAlarm->alarmAction.audioAction.enable.timespan_num = time_num;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else if(ActID == 29)//只获取周一的时间段进行设置
            {
                int Enabled_int = 0;
                cJSON *Enabled = cJSON_GetObjectItem(LinkageActionInfo, "Enabled");
                if (Enabled != NULL)
                {
                    Enabled_int = Enabled->valueint;
                    if (Enabled_int == 0)
                        pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag = 0;
                }

                __DBG("pd action light_twinkle enable:%d\n", pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag);

                cJSON *ActParam = cJSON_GetObjectItem(LinkageActionInfo, "ActParam");
                if (ActParam != NULL && Enabled_int != 0)
                {
                    cJSON *LightCustomInfo = cJSON_GetObjectItem(ActParam, "LightCustomInfo");
                    if (LightCustomInfo != NULL)
                    {
                        cJSON *LightWeekPlanInfo = cJSON_GetObjectItem(LightCustomInfo, "LightWeekPlanInfo");
                        if (LightWeekPlanInfo != NULL)
                        {
                            cJSON *Days = cJSON_GetObjectItem(LightWeekPlanInfo, "Days");
                            if (Days != NULL)
                            {
                                cJSON *Days_1 = cJSON_GetArrayItem(Days, 0);//获取周一
                                if (Days_1 != NULL)
                                {
                                    cJSON *TimeSectionInfos = cJSON_GetObjectItem(Days_1, "TimeSectionInfos");
                                    if (TimeSectionInfos != NULL)
                                    {
                                        cJSON * Num = cJSON_GetObjectItem(Days_1, "Num");
                                        if (Num != NULL)
                                        {
                                            int Num_int = Num->valueint;
                                            if (Num_int == 4 && Enabled_int != 0)
                                            {
                                                pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag = ARMING_CUSTOM;
                                                int time_num = 0;

                                                for (j = 0; j < 4; j++)
                                                {
                                                    cJSON *TimeSectionInfos_i = cJSON_GetArrayItem(TimeSectionInfos, j);
                                                    if (TimeSectionInfos_i != NULL)
                                                    {
                                                        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfos_i, "Begin");
                                                        cJSON *End = cJSON_GetObjectItem(TimeSectionInfos_i, "End");
                                                        if (Begin != NULL && End != NULL)
                                                        {
                                                            char *BeginStr = Begin->valuestring;
                                                            char *EndStr = End->valuestring;
                                                            __DBG("BeginStr:%s, EndStr:%s\n", BeginStr, EndStr);

                                                            int bhour, bminute, bsec, ehour, eminute, esec;
                                                            int ret1 = sscanf(BeginStr, "%d:%d:%d", &bhour, &bminute, &bsec);
                                                            int ret2 = sscanf(EndStr, "%d:%d:%d", &ehour, &eminute, &esec);
                                                            if (ret1 == 3 && ret2 == 3)
                                                            {
                                                                __DBG("Begin time:%d:%d:%d, End time:%d:%d:%d\n",bhour, bminute, bsec, ehour, eminute, esec);
                                                                //if (bhour != 0 || bminute != 0 || bsec != 0 || ehour != 0 || eminute != 0 || esec != 0)
                                                                {
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.hour = bhour;
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.minute = bminute;
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].startTime.sec = bsec;
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.hour = ehour;
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.minute = eminute;
                                                                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[j].endTime.sec = esec;
                                                                    time_num ++;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                __DBG("time_num:%d\n", time_num);
                                                pstPdAlarm->alarmAction.light_twinkle_enable.timespan_num = time_num;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }


        {
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                anj_config_video_capture_set(&stVideoCapture, iCameraIdx);
            }
        }

        {
            anj_config_audio_capture_set(&stAudioCapture);
        }

        {
            PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
            for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
            {
                memcpy(&stPdAlarmArray[iCameraIdx], pstPdAlarm, sizeof(PdAlarm));
            }
            anj_config_alarm_pd_set(stPdAlarmArray);
        }

    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstPdAlarm != NULL)
    {
        anj_mw_free(pstPdAlarm);
        pstPdAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pLinkageActionJson != NULL)
    {
        cJSON_Delete(pLinkageActionJson);
        pLinkageActionJson = NULL;
    }

    return HTTP_PUT_OK;
}

// smart motion -> human
int unv_alarm_motion_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    char *pAreaStr = NULL;

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pRectAreasListJson = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pRectAreasListJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *RECTAreas = cJSON_GetObjectItem(pRectAreasListJson, "RECTAreas");
    cJSON *Num = cJSON_GetObjectItem(pRectAreasListJson, "Num");

    if (Num != NULL)
    {
        int Num_int = Num->valueint;
        if(Num_int == 1) 
        {
            cJSON *pAreas =(cJSON_GetArrayItem(RECTAreas, 0));
            pAreaStr =  cJSON_Print(pAreas);//free 4
            alarm_motion_area_set(pAreaStr); 
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRectAreasListJson != NULL)
    {
        cJSON_Delete(pRectAreasListJson);
        pRectAreasListJson = NULL;
    }

    if (pAreaStr != NULL)
    {
        anj_mw_free(pAreaStr);
        pAreaStr = NULL;
    }

    return HTTP_PUT_OK;
}


/***************** unv smart set *************************/

//设置工作状态
int unv_smart_work_status_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    //0-FACE;107-SMART;101-102-103-区域入侵、进入区域、离开区域;100-GATE

    __DBG("unv smart work status set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarm = {0};
    FaceDetectAlarm stFaceDetectAlarm = {0};
    VideoRegionAiAlarm stRegionAlarm = {0};
    VideoGateAlarm stVideoGateAlarm = {0};

    memcpy(&stPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));
    memcpy(&stFaceDetectAlarm, &pstAlarmConfig->aiAlarm.fdAlarm[iCameraIdx], sizeof(FaceDetectAlarm));
    memcpy(&stRegionAlarm, &pstAlarmConfig->aiAlarm.regionAiAlarm[iCameraIdx], sizeof(VideoRegionAiAlarm));
    memcpy(&stVideoGateAlarm, &pstAlarmConfig->aiAlarm.vgAlarm[iCameraIdx], sizeof(VideoGateAlarm));

    int i = 0;
    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pWorkStatusJson  = cJSON_Parse(pMsgBody); //free 2

    if(Response == NULL || pWorkStatusJson  == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *DisEnableIDs = cJSON_GetObjectItem(pWorkStatusJson , "DisableIDs");
    if (DisEnableIDs != NULL)
    {
        int Num = cJSON_GetArraySize(DisEnableIDs);

        for (i = 0; i < Num; i++)
        {
            cJSON *ID_t = cJSON_GetArrayItem(DisEnableIDs, i);
            int ID_int = ID_t->valueint;
            if (ID_int == 0)
            {
                stFaceDetectAlarm.enable = 0;
            }
            if (ID_int == 3)
            {
                stPdAlarm.alarmAction.track_human_enable = 0;
            }
            else if (ID_int == 107)
            {
                stPdAlarm.enable = 0;
            }
            else if (ID_int == 101)
            {
                stRegionAlarm.enable = 0;
                stRegionAlarm.data[0].enable = 0;
            }
            else if (ID_int == 102)
            {
                stRegionAlarm.enable = 0;
                stRegionAlarm.data[1].enable = 0;
            }
            else if (ID_int == 103)
            {
                stRegionAlarm.enable = 0;
                stRegionAlarm.data[2].enable = 0;
            }
            else if (ID_int == 100)
            {
                stVideoGateAlarm.enable = 0;
            }
        }
    }

    //cJSON *EnableNum = cJSON_GetObjectItem(WorkingStatusInfo , "EnableNum");
    cJSON *EnableIDs = cJSON_GetObjectItem(pWorkStatusJson , "EnableIDs");
    if (EnableIDs != NULL)
    {
        int Num = cJSON_GetArraySize(EnableIDs);
        for (i = 0; i < Num; i++)
        {
            cJSON *ID_t = cJSON_GetArrayItem(EnableIDs, i);
            int ID_int = ID_t->valueint;
            if (ID_int == 0)
            {
                stFaceDetectAlarm.enable = 1;
            }
            if (ID_int == 3)
            {
                stPdAlarm.enable = 1;
                stPdAlarm.alarmAction.track_human_enable = 1;
            }
            else if (ID_int == 107)
            {
                stPdAlarm.enable = 1;
            }
            else if (ID_int == 101)
            {
                stRegionAlarm.enable = 1;
                stRegionAlarm.data[0].enable = 1;
            }
            else if (ID_int == 102)
            {
                stRegionAlarm.enable = 1;
                stRegionAlarm.data[1].enable = 1;
            }
            else if (ID_int == 103)
            {
                stRegionAlarm.enable = 1;
                stRegionAlarm.data[2].enable = 1;
            }
            else if (ID_int == 100)
            {
                stVideoGateAlarm.enable = 1;
            }
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }

        anj_config_alarm_pd_set(stPdAlarmArray);
        usleep(10 * 1000);
    }

    {
        FaceDetectAlarm stFaceAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stFaceAlarmArray[iCameraIdx], &stFaceDetectAlarm, sizeof(FaceDetectAlarm));
        }

        anj_config_alarm_fd_set(stFaceAlarmArray);
        usleep(10 * 1000);
    }

    {
        VideoRegionAiAlarm stRegionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stRegionAlarmArray[iCameraIdx], &stRegionAlarm, sizeof(VideoRegionAiAlarm));
        }

        anj_config_alarm_region_set(stRegionAlarmArray);
        usleep(10 * 1000);
    }

    {
        VideoGateAlarm stVideoGateAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stVideoGateAlarmArray[iCameraIdx], &stVideoGateAlarm, sizeof(VideoGateAlarm));
        }

        anj_config_alarm_video_gate_set(stVideoGateAlarmArray);
        usleep(10 * 1000);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWorkStatusJson != NULL)
    {
        cJSON_Delete(pWorkStatusJson);
        pWorkStatusJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_system_photoserver_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pPhotoServerJson = cJSON_Parse(pMsgBody); //free 2
    if(Response == NULL || pPhotoServerJson  == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pPhotoServerJson != NULL)
    {
        cJSON_Delete(pPhotoServerJson);
        pPhotoServerJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_attribute_collect_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pAttributeJson  = cJSON_Parse(pMsgBody); //free 2
    if(Response == NULL || pAttributeJson  == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAttributeJson != NULL)
    {
        cJSON_Delete(pAttributeJson);
        pAttributeJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_alarm_human_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv alarm human weekplan set\n");

    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    PdAlarm stPdAlarm = {0};
    memcpy(&stPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));


    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); // free 2

    if(Response == NULL || pWeekPlanJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int i;
    int Num_int;
    int Enabled_int;

    cJSON *Enabled = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    cJSON *Num = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    cJSON *Days = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    if (Enabled != NULL && Num != NULL && Days != NULL)
    {
        Enabled_int = Enabled->valueint;
        Num_int = Num->valueint;
        if (Enabled_int == 0) 
        {
            stPdAlarm.arming_flag = 0;
            for (i = 0; i < 7; i++)
                stPdAlarm.timeSpan.workday[i] = 0;
        }
        else
        {
            stPdAlarm.arming_flag = 4;
            for (i = 0; i < Num_int; i++) 
            {
                cJSON *Days_i = cJSON_GetArrayItem(Days, i);// free 6
                alarm_day_plan_set(Days_i, &stPdAlarm.timeSpan); 
            }
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], &stPdAlarm, sizeof(PdAlarm));
        }

        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    return HTTP_PUT_OK;
}

int alarm_time_section_ex_set(int ID, int Num, cJSON *TimeSectionInfos, TimeSpanCfg *pTimeSpan)
{
    int i, B, E;
    char *BeginStr = NULL;
    char *EndStr = NULL;

    char plan[33] = {'0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', 
                    '0', '0', '0', '0', '0', '0', '0', '0', '\0'};

    for (i = 0; i < Num; i++)
    {
        cJSON *TimeSectionInfo = cJSON_GetArrayItem(TimeSectionInfos, i);
        cJSON *Begin = cJSON_GetObjectItem(TimeSectionInfo, "Begin");
        cJSON *End = cJSON_GetObjectItem(TimeSectionInfo, "End");
        //cJSON *ArmingType  = cJSON_GetObjectItem(TimeSectionInfo, "ArmingType");

        BeginStr = cJSON_Print(Begin);// free 3
        B = Get_time(BeginStr); 
        EndStr = cJSON_Print(End); // free 4
        if (!strcmp(EndStr, "\"23:59:59\""))
        {
            E = 24;
        }
        else
        {
            E = Get_time(EndStr);
        }

        __DBG("BeginStr:%s, EndStr:%s, B:%d, E:%d\n", BeginStr, EndStr, B, E);
        Set_one_zero(B, E, plan);        
    }

    if (ID == 7)
    {
        pTimeSpan->workday[0] = ToInt(plan);
    }
    else
    {
        pTimeSpan->workday[ID] = ToInt(plan);
    }

    if (BeginStr != NULL)
    {
        anj_mw_free(BeginStr);
        BeginStr = NULL;
    }

    if (EndStr != NULL)
    {
        anj_mw_free(EndStr);
        EndStr = NULL;
    }

    return 0;
}

//设置天
int alarm_day_plan_ex_set(cJSON *DayPlan, TimeSpanCfg *pTimeSpan)
{
    cJSON *ID = cJSON_GetObjectItem(DayPlan, "ID");
    cJSON *Num = cJSON_GetObjectItem(DayPlan, "Num");
    cJSON *TimeSectionInfos = cJSON_GetObjectItem(DayPlan, "TimeSectionInfos");

    int ID_int = ID->valueint;
    int Num_int = Num->valueint;

    if (TimeSectionInfos != NULL)
    {
        alarm_time_section_ex_set(ID_int, Num_int, TimeSectionInfos, pTimeSpan);
    }

    return 0;
}

int unv_alarm_motion_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv alarm motion week plan set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    MotionDetectAlarm *pstNewMotionAlarm = (MotionDetectAlarm *) anj_mw_malloc(sizeof(MotionDetectAlarm));
    if (pstNewMotionAlarm == NULL)
    {
        __ERR("pstNewMotionAlarm malloc failed\n");
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstNewMotionAlarm, &pAlarmConfig->normalAlarm.motionDetectAlarm[iCameraIdx], sizeof(MotionDetectAlarm));

    char *Enabled_pri = NULL;

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); // free 2

    if(Response == NULL || pWeekPlanJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int Num_t = -1;// free 5
    int i = 0;

    cJSON *Enabled = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    cJSON *Num = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    cJSON *Days = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    if (Enabled != NULL && Num != NULL)
    {
        Enabled_pri = cJSON_Print(Enabled);// free 4
        Num_t = Num->valueint;
        if (!strcmp("false", Enabled_pri)) 
        {
            pstNewMotionAlarm->arming_flag = 0;
            for (i = 0; i < 7; i++)
                pstNewMotionAlarm->timeSpan.workday[i] = 0;
        }
        else
        {
            pstNewMotionAlarm->arming_flag = 4;
            for (i = 0; i < Num_t; i++) 
            {
                cJSON *Days_i = cJSON_GetArrayItem(Days, i);// free 6
                alarm_day_plan_ex_set(Days_i, &pstNewMotionAlarm->timeSpan); 
            }
        }
    }

    {
        MotionDetectAlarm stMotionAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stMotionAlarmArray[iCameraIdx], pstNewMotionAlarm, sizeof(MotionDetectAlarm));
        }

        anj_config_alarm_motion_set(stMotionAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstNewMotionAlarm != NULL)
    {
        anj_mw_free(pstNewMotionAlarm);
        pstNewMotionAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    if (Enabled_pri != NULL)
    {
        anj_mw_free(Enabled_pri);
        Enabled_pri = NULL;
    }

    return HTTP_PUT_OK;
}

/*int Set_WeekPlan(PdAlarm *PdAlarmCfg, char *Week)
{
//-----------------------------------------------------
char *str = malloc(33);
char *str1 = malloc(24);
memset(str, 0, 33);    
memset(str1, 0, 24);
str[32] = '\0';
cJSON *Week_Json;
cJSON *Days_arry;
int day, time;
//1.初始化week 
Week_Json = cJSON_CreateObject();
Days_arry = cJSON_CreateArray();
cJSON_AddStringToObject(Week_Json, "Enable", "1");
int Week_Num = 0;
char Week_Num_s[8] = "%d";
int Num = 0;
//-----------------------------------------------------
for (day = 0; day < 7; day++){
//-------------------------------------------
ToBin(PdAlarmCfg->timeSpan.workday[day], str);
strcpy(str1, str+8);
if (strcmp(str1, "000000000000000000000000")){
//1.初始化DAY/ 
cJSON *Day_Json;
cJSON *TimeSectionInfos_arry;
Day_Json=cJSON_CreateObject();
TimeSectionInfos_arry = cJSON_CreateArray();
char Num_s[8] = "%d";
char ID_s[8] = "%d";
sprintf(ID_s, ID_s, day+1);
cJSON_AddStringToObject(Day_Json, "ID", ID_s);
Num = 0;
time = 0;
//-------------------------------------------
while(time < 24){
if (str1[23 - time] == 49)
{    
//1.TimeSectionInfo
cJSON *Time_Json;
char Begin[16] = "%2d:00:00";
char End[16] = "%2d:00:00";
Time_Json=cJSON_CreateObject();
cJSON_AddStringToObject(Time_Json, "ArmingType", "0");
Num++;
sprintf(Begin, Begin, time);
cJSON_AddStringToObject(Time_Json, "Begin", Begin);
while (time < 24){
if (str1[23 - time] == 48){
sprintf(End, End, time);
cJSON_AddStringToObject(Time_Json, "End", End);
break;
}
else if(time == 23){
sprintf(End, End, time+1);
cJSON_AddStringToObject(Time_Json, "End", End);
break;
}                
else
time++;
}
time++;
cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
}
else {
time++;
}
}
Week_Num++;
sprintf(Num_s, Num_s, Num);
cJSON_AddStringToObject(Day_Json, "Num", Num_s);            
//3.结构体嵌套填充
//向cJSON结构体province中添加cityArray数组对象
cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
cJSON_AddItemToArray(Days_arry, Day_Json);
}
}
//3.结构体嵌套填充
//向cJSON结构体province中添加cityArray数组对象
sprintf(Week_Num_s, Week_Num_s, Week_Num);
cJSON_AddStringToObject(Week_Json, "Num", Week_Num_s);    
cJSON_AddItemToObject(Week_Json, "Days", Days_arry);

memcpy(Week, cJSON_Print(Week_Json), strlen(cJSON_Print(Week_Json)));

//HTTP_LOG("Week:%s\n", Week);
//HTTP_LOG("Week_Json:%s\n", cJSON_Print(Week_Json));
free(str);
free(str1);
return 0;
//-----------------------------------------------------
}*/



/*
{
"Num":1,
"RectangleAreasList":[{
"ID":"0",
"Enabled":1,
"Sensitivity":6,
"TargetSize":4,
"Area": {
"TopLeft":{
"X":0,
"Y":0
},
"BottomRight":{
"X":10000,
"Y":10000
}
}
}]
}
*/

int unv_alarm_human_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    __DBG("unv alarm human areas set\n");
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    PdAlarm *pstPdAlarm = (PdAlarm *)anj_mw_malloc(sizeof(PdAlarm));
    if (pstPdAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    int enable = 0;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    cJSON *jsonRectangleAreasList = NULL;
    cJSON *jsonitem = NULL;
    cJSON *jsonarea = NULL;
    cJSON *jsonTopLeft = NULL;
    cJSON *jsonBottomRight = NULL;

    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pAreasJson = cJSON_Parse(pMsgBody); // free 2

    if(pAreasJson == NULL || Response == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    jsonRectangleAreasList = cJSON_GetObjectItem(pAreasJson, "RectangleAreasList");
    if(jsonRectangleAreasList == NULL)
    {
        status = 304;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    jsonitem = cJSON_GetArrayItem(jsonRectangleAreasList, 0);
    if(jsonitem == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    if(cJSON_GetObjectItem(jsonitem, "Enabled"))
    {
        enable = cJSON_GetObjectItem(jsonitem, "Enabled")->valueint;
    }

    if(enable)
    {
        if(cJSON_GetObjectItem(jsonitem, "Sensitivity"))
        {
            pstPdAlarm->sensitivity = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint / 10;
            pstPdAlarm->threshold = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint % 10;

            pstPdAlarm->sensitivitys[AI_TYPE_BIT_CAR] = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint;
            pstPdAlarm->sensitivitys[AI_TYPE_BIT_MOTO] = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint;
            pstPdAlarm->sensitivitys[AI_TYPE_BIT_ELECTRICBICYCLE] = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint;
            pstPdAlarm->sensitivitys[AI_TYPE_BIT_BICYCLE] = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint;
            pstPdAlarm->sensitivitys[AI_TYPE_BIT_HUMAN] = cJSON_GetObjectItem(jsonitem, "Sensitivity")->valueint;
        }
        //if(cJSON_GetObjectItem(jsonitem, "Type"))
        //pdAlmCfg.type = cJSON_GetObjectItem(jsonitem, "Type")->valueint;

        jsonarea = cJSON_GetObjectItem(jsonitem, "Area");
        if(jsonarea)
        {
            jsonTopLeft = cJSON_GetObjectItem(jsonarea, "TopLeft");
            jsonBottomRight = cJSON_GetObjectItem(jsonarea, "BottomRight");             
        }

        if(jsonarea && jsonTopLeft && jsonBottomRight)
        {
            if(cJSON_GetObjectItem(jsonTopLeft, "X") && cJSON_GetObjectItem(jsonTopLeft, "Y"))
            {
                x1 = cJSON_GetObjectItem(jsonTopLeft, "X")->valueint;
                y1 = cJSON_GetObjectItem(jsonTopLeft, "Y")->valueint;
            }

            if(cJSON_GetObjectItem(jsonBottomRight, "X") && cJSON_GetObjectItem(jsonBottomRight, "Y"))
            {
                x2 = cJSON_GetObjectItem(jsonBottomRight, "X")->valueint;
                y2 = cJSON_GetObjectItem(jsonBottomRight, "Y")->valueint;
            }

            pstPdAlarm->polygonArea.count = 4;
            pstPdAlarm->polygonArea.points[0].x = x1 / 100;
            pstPdAlarm->polygonArea.points[0].y = y1 / 100;
            pstPdAlarm->polygonArea.points[1].x = x2 / 100;
            pstPdAlarm->polygonArea.points[1].y = y1 / 100;
            pstPdAlarm->polygonArea.points[2].x = x2 / 100;
            pstPdAlarm->polygonArea.points[2].y = y2 / 100;
            pstPdAlarm->polygonArea.points[3].x = x1 / 100;
            pstPdAlarm->polygonArea.points[3].y = y2 / 100;
        }
    }
    else
    {
        pstPdAlarm->polygonArea.count = 0;
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], pstPdAlarm, sizeof(PdAlarm));
        }

        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (pstPdAlarm != NULL)
    {
        anj_mw_free(pstPdAlarm);
        pstPdAlarm = NULL;
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pAreasJson != NULL)
    {
        cJSON_Delete(pAreasJson);
        pAreasJson = NULL;
    }

    return HTTP_PUT_OK;
}

/*
{
"Enabled":    1
}    
*/
int unv_alarm_human_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    PdAlarm *pstPdAlarm = (PdAlarm *)anj_mw_malloc(sizeof(PdAlarm));
    if (pstPdAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(ResponseLen); // free 1
    cJSON *pRuleJson = cJSON_Parse(pMsgBody); // free 2
    if(pRuleJson == NULL || Response == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *Enabled = NULL;
    int Enabled_t = 0;

    Enabled = cJSON_GetObjectItem(pRuleJson, "Enabled");
    if (Enabled != NULL)
    {
        Enabled_t = Enabled->valueint;
    }

    if (Enabled_t == 1) 
    {
        pstPdAlarm->type |= (1<<4);
        pstPdAlarm->type |= (1<<3);
        pstPdAlarm->type |= (1<<2);
        pstPdAlarm->type |= (1<<1);
        pstPdAlarm->enable  = 1;
        //g_yen_version_pd_clock = 1;
    }
    else
    {
        pstPdAlarm->type &= (~(1<<4));
        pstPdAlarm->type &= (~(1<<3));
        pstPdAlarm->type &= (~(1<<2));
        pstPdAlarm->type &= (~(1<<1));
        //g_yen_version_pd_clock = 0;
        //if (g_yen_version_car_clock == 0 && g_yen_version_pd_clock == 0)
        {
            pstPdAlarm->enable = 0;
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], pstPdAlarm, sizeof(PdAlarm));
        }

        anj_config_alarm_pd_set(stPdAlarmArray);
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pRuleJson != NULL)
    {
        cJSON_Delete(pRuleJson);
        pRuleJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_smart_autotrack_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)//使能智能侦测，关闭移动侦测
{
    if(pInst == NULL || pMsgBody == NULL)
        return -1;

    PdAlarm *pstPdAlarm = (PdAlarm *)anj_mw_malloc(sizeof(PdAlarm));
    if (pstPdAlarm == NULL)
    {
        return -1;
    }

    int iCameraIdx = 0;
    AlarmConfig *pstAlarmConfig = (AlarmConfig *)getAlarmConfig();
    memcpy(pstPdAlarm, &pstAlarmConfig->aiAlarm.pdAlarm[iCameraIdx], sizeof(PdAlarm));

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(sizeof(char)*ResponseLen);

    cJSON *TrackingRuleJson = cJSON_Parse(pMsgBody);
    if(Response == NULL || TrackingRuleJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *RuleJson = cJSON_GetObjectItem(TrackingRuleJson, "Rule");
    if (RuleJson == NULL) 
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    cJSON *Enabled = cJSON_GetObjectItem(RuleJson, "Enabled");
    cJSON *Mode = cJSON_GetObjectItem(RuleJson, "Mode");
    cJSON *OverAll = cJSON_GetObjectItem(RuleJson, "OverAll");

    if (Enabled != NULL)
    {
        int Enabled_int = Enabled->valueint;
        if (Enabled_int == 1)
        {
            pstPdAlarm->enable = 1;
            pstPdAlarm->alarmAction.track_human_enable = 1;
            pstPdAlarm->arming_flag = ARMING_ALLDAY;
        }
        else
        {
            pstPdAlarm->alarmAction.track_human_enable = 0;
        }
    }

    if (Mode != NULL)
    {
        int Mode_int = Mode->valueint;
        if (Mode_int == 1)
        {
            if (OverAll != NULL)
            {
                cJSON *TrackTime = cJSON_GetObjectItem(OverAll, "TrackTime");
                if (TrackTime != NULL)
                {
                    int tracktime_int = TrackTime->valueint;
                    pstPdAlarm->alarmAction.track_time = (tracktime_int > 0) ? tracktime_int : 0;
                }
            }
        }
    }

    {
        PdAlarm stPdAlarmArray[ANJ_CAMERA_MAX_NUMS] = {0};
        for (iCameraIdx = 0; iCameraIdx < ANJ_CAMERA_MAX_NUMS; iCameraIdx++)
        {
            memcpy(&stPdAlarmArray[iCameraIdx], pstPdAlarm, sizeof(PdAlarm));
        }

        anj_config_alarm_pd_set(stPdAlarmArray);
    }


    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(NULL != pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if ( NULL != pstPdAlarm)
    {
        anj_mw_free(pstPdAlarm);
        pstPdAlarm = NULL;
    }

    if ( NULL != Response)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    return HTTP_PUT_OK;
}

//-------------------------------------------------------------------------------------------
//                          subcription
//-------------------------------------------------------------------------------------------

int unv_subcription_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    char *Response = (char *)anj_mw_malloc(sizeof(char) * ResponseLen);
    cJSON *pSubcriptionJson = cJSON_Parse(pMsgBody);

    if(Response == NULL || pSubcriptionJson == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT1;
    }

    int iRet = 0;
    int port = 0;
    int id_num = s_unv_sub_id_num;

    char header_buf[256] = {0};
    char *pheader = NULL;
    pheader = (char *)anj_mw_malloc(1024);
    if (pheader == NULL)
    {
        __ERR("pheader malloc failed\n");
        goto __EXIT1;
    }

    memset(pheader, 0, 1024);
    snprintf(header_buf, sizeof(header_buf), "POST /LAPI/V1.0/System/Event/Subscription/%d\n", id_num);
    strcpy(pheader, header_buf);

    cJSON *root         = cJSON_CreateObject();
    //cJSON *AddressType    = cJSON_GetObjectItem(Input, "AddressType");
    cJSON *IPAddress    = cJSON_GetObjectItem(pSubcriptionJson, "IPAddress");  
    cJSON *Port         = cJSON_GetObjectItem(pSubcriptionJson, "Port");
    cJSON *Duration     = cJSON_GetObjectItem(pSubcriptionJson, "Duration");

    char *ip_buf = NULL;
    if (IPAddress != NULL && Port != NULL)
    {
        ip_buf = IPAddress->valuestring;
        port = Port->valueint;
    }
    else
    {
        __ERR("get IPAddress or Port failed\n");
        status = HTTP_RES_STATUS_NOT_MODIFY;
        if (NULL != pheader)
        {
            anj_mw_free(pheader);
            pheader = NULL;
        }

        if ( NULL != root )
        {
            cJSON_Delete(root);
            root = NULL;
        }
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT1;
    }

    char ip_addr[64] = {0};
    int web_port = 0;
    http_get_ip_addr_port(ip_addr, sizeof(ip_addr), &web_port);

    char Reference[128] = {0};
    snprintf(Reference, sizeof(Reference), "%s:%d/Subscription/Subscribers/%d", ip_addr, web_port, s_unv_sub_id_num);

    long time_start = time(NULL);
    long Duration_int = 120;
    if (NULL != Duration)
    {
        Duration_int = Duration->valueint;
    }

    long time_end = time_start + Duration_int;

    cJSON_AddNumberToObject(root, "ID", s_unv_sub_id_num);
    cJSON_AddStringToObject(root, "Reference", Reference);
    cJSON_AddNumberToObject(root, "CurrentTime", time_start);
    cJSON_AddNumberToObject(root, "TerminationTime", time_end);
    cJSON_AddNumberToObject(root, "SupportType", 8);

    char *root_pri = cJSON_Print(root);
    strcat(pheader, root_pri);

    char str_ip[64] = {0};
    snprintf(str_ip, sizeof(str_ip), "%s:%d", ip_buf, port);

    //将IP加入订阅队伍
    iRet = unv_subscribe_add(ip_buf, port, Duration_int, id_num);
    if (iRet != 0)
    {
        __ERR("subscribe add failed!\n");
        status = 304;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT2;
    }

    s_unv_sub_id_num++;
    snprintf(Response, ResponseLen, ResponseOKStr, http_url, root_pri);

__EXIT2:
    if( NULL != pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if ( NULL != Response)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if ( NULL != pSubcriptionJson)
    {
        cJSON_Delete(pSubcriptionJson);
        pSubcriptionJson = NULL;
    }

    if ( NULL != pheader)
    {
        anj_mw_free(pheader);
        pheader = NULL;
    }

    if ( NULL != root )
    {
        cJSON_Delete(root);
        root = NULL;
    }

    if ( NULL != root_pri)
    {
        free(root_pri);
        root_pri = NULL;
    }
    return HTTP_PUT_OK;

__EXIT1:
    if( NULL != pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if ( NULL != Response )
    {
        free(Response);
        Response = NULL;
    }

    if ( NULL != pSubcriptionJson )
    {
        cJSON_Delete(pSubcriptionJson);
        pSubcriptionJson = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_subcription_refresh_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
        return -1;

    char *root_pri = NULL;
    cJSON *root = NULL;

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr)+1024;
    Response = (char *)anj_mw_malloc(sizeof(char)*ResponseLen);

    cJSON *Input = NULL;
    if (pMsgBody != NULL)
    {
        Input = cJSON_Parse(pMsgBody);
    }

    if(Input == NULL || Response == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT1;
    }

    char *pp = strstr(http_url, "Subscription/");
    pp += 13;

    int id_num = atoi(pp);
    int refresh_seconds = 90;
    cJSON *Duration = cJSON_GetObjectItem(Input, "Duration");
    if (NULL != Duration)
    {
        refresh_seconds = Duration->valueint;
    }

    if (refresh_seconds == 0)
    {
        status = 304;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    int iRet = 0;;
    Unv_Sub_Node *pGetNode = NULL;
    root = cJSON_CreateObject();

    iRet = unv_subscribe_refresh(refresh_seconds, id_num, &pGetNode);
    if (iRet == 0 && pGetNode != NULL)
    {
        char ip_addr[64] = {0};
        int web_port = 0;
        http_get_ip_addr_port(ip_addr, sizeof(ip_addr), &web_port);
    
        char Reference[256] = {0};
        sprintf(Reference, "%s:%d/Subscription/Subscribers/%d", ip_addr, web_port, id_num);
        cJSON_AddStringToObject(root, "Reference", Reference);
    }

    long cur_time = time(0);
    long time_end = cur_time + refresh_seconds;

    cJSON_AddNumberToObject(root, "CurrentTime", cur_time);
    cJSON_AddNumberToObject(root, "TerminationTime", time_end);

    root_pri = cJSON_Print(root);
    snprintf(Response, ResponseLen, ResponseOKStr, http_url, root_pri);

__EXIT:
    if( NULL != pCbResponse)
        pCbResponse(pInst, Response, status);

    if (NULL != Response)
    {
        anj_mw_free(Response);
        Response = NULL;
    }
    if (NULL != Input)
    {
        cJSON_Delete(Input);
        Input = NULL;
    }
    if (NULL != root)
    {
        cJSON_Delete(root);
        root = NULL;
    }
    if ( NULL != root_pri)
    {
        anj_mw_free(root_pri);
        root_pri = NULL;
    }
    return HTTP_PUT_OK;

__EXIT1:
    if( NULL != pCbResponse)
        pCbResponse(pInst, Response, status);
    if ( NULL != Response)
    {
        anj_mw_free(Response);
        Response = NULL;
    }
    if (NULL != Input)
    {
        cJSON_Delete(Input);
        Input = NULL;
    }

    return HTTP_PUT_OK;
}

int unv_subcription_delete_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL)
    {
        return -1;
    }

    //unsigned int id;
    //sscanf(http_url, "/LAPI/V1.0/System/Event/Subscription/%d", &id);
    //UnvSubsMap_delete(id);
    
    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;
    Response = (char *)anj_mw_malloc(sizeof(char)*ResponseLen);
    if(Response == NULL)
    {
        status = 304;
        goto __EXIT;
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if( NULL != pCbResponse)
        pCbResponse(pInst, Response, status);

    if (NULL != Response)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    return HTTP_PUT_OK;
}

/*
{
"Num":    7,
"Enabled":    1,
"Days":    [{
"ID":    1,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    2,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    3,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    4,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    5,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    6,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}, {
"ID":    7,
"Num":    1,
"TimeSectionInfos":    [{
"Begin":    "00:00:00",
"End":    "23:59:59"
}]
}]
}
*/

#if 0
int unv_alarm_human_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse)
{
    if(pInst == NULL || pMsgBody == NULL)
    {
        return -1;
    }

    int status = HTTP_RES_STATUS_OK;
    char *Response = NULL;
    int ResponseLen = sizeof(ResponseOKStr) + 1024;

    cJSON *pjsonNum = NULL;
    cJSON *pjsonEnable = NULL;
    cJSON *pjsonDays = NULL;

    Response = (char *)anj_mw_malloc(ResponseLen); //free 1
    cJSON *pWeekPlanJson = cJSON_Parse(pMsgBody); //free 2
    if(pWeekPlanJson == NULL || Response == NULL)
    {
        status = HTTP_RES_STATUS_NOT_MODIFY;
        snprintf(Response, ResponseLen, ResponseFailStr, http_url, status, "\"null\"");
        goto __EXIT;
    }

    pjsonNum = cJSON_GetObjectItem(pWeekPlanJson, "Num");
    pjsonEnable = cJSON_GetObjectItem(pWeekPlanJson, "Enabled");
    pjsonDays = cJSON_GetObjectItem(pWeekPlanJson, "Days");

    if(pjsonNum && pjsonNum->type == cJSON_Number && 
        pjsonEnable && pjsonEnable->type == cJSON_Number && 
        pjsonDays && pjsonDays->type == cJSON_Array)
    {
        int i = 0;
        int num = pjsonNum->valueint;
        int enable = pjsonEnable->valueint;
        cJSON *item = NULL;

        AlarmConfig *pAlarmCfg = (AlarmConfig *)getAlarmConfig();
        PdAlarm *pPdAlarmCfg = &pAlarmCfg->aiAlarm.pdAlarm[0];
        
        PdAlarm stPdOldCfg = {0};
        memcpy(&stPdOldCfg, pPdAlarmCfg, sizeof(PdAlarm));

        TimeSpanList *pTimeSpanList = NULL;
        WorkDayTime *pWorkDayTime = NULL;

        if(num > cJSON_GetArraySize(pjsonDays))
            num = cJSON_GetArraySize(pjsonDays);

        TimeSpanList timeSpanList;
        pTimeSpanList = &(timeSpanList);
        memset(pTimeSpanList, 0, sizeof(TimeSpanList));        
        pTimeSpanList->workdayCnt = num;

        for(i = 0; i < num; i++)
        {
            if(!enable)
                break;

            int j = 0;
            cJSON *jsonTimeSectionInfos = NULL;
            cJSON *Begin = NULL;
            cJSON *End = NULL;
            cJSON *TimeSectionItem = NULL;

            pWorkDayTime = &(pTimeSpanList->workdayTimes[i]);
            pWorkDayTime->workday = i;

            item = cJSON_GetArrayItem(pjsonDays, i);
            jsonTimeSectionInfos = cJSON_GetObjectItem(item, "TimeSectionInfos");

            if(jsonTimeSectionInfos)
            {
                pWorkDayTime->timeSpancnt = cJSON_GetArraySize(jsonTimeSectionInfos);

                for(j = 0; j < pWorkDayTime->timeSpancnt; j++)
                {
                    if((TimeSectionItem = cJSON_GetArrayItem(jsonTimeSectionInfos, j)) == NULL)
                        continue;

                    Begin = cJSON_GetObjectItem(TimeSectionItem, "Begin");
                    End = cJSON_GetObjectItem(TimeSectionItem, "End");

                    if(Begin == NULL || End == NULL)
                        continue;

                    setTimeSpanByTimeIntervalStr(&(pWorkDayTime->timeSpans[j]), Begin->valuestring, End->valuestring);
                }
            }    
        }

        TransTimeSpan2New(&timeSpanList, &pPdAlarmCfg->timeSpan);

        int cameraIndex = 0;
        if (memcmp(&stPdOldCfg, pPdAlarmCfg, sizeof(PdAlarm)) != 0)
        {
            for (cameraIndex = 0; cameraIndex < ANJ_CAMERA_MAX_NUMS; cameraIndex++)
            {
                anj_config_alarm_pd_set(pPdAlarmCfg, cameraIndex);
            }
        }
    }

    snprintf(Response, ResponseLen, ResponseOKStr, http_url, "\"null\"");

__EXIT:
    if(pCbResponse)
    {
        pCbResponse(pInst, Response, status);
    }

    if (Response != NULL)
    {
        anj_mw_free(Response);
        Response = NULL;
    }

    if (pWeekPlanJson != NULL)
    {
        cJSON_Delete(pWeekPlanJson);
        pWeekPlanJson = NULL;
    }

    return HTTP_PUT_OK;
}
#endif
