#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "anj_mw_comm.h"
#include "anj_mw_net.h"

#include "anj_config.h"
#include "anj_sysctl.h"
#include "function_list.h"

#include "cJSON.h"
#include "http_handle.h"
#include "http_unv.h"
#include "unv_def.h"
#include "unv_put.h"
#include "unv_get.h"

static char unv_str_alarm_human_enable_str[] = 
"{\n"
"\t\"Enabled\":\t%d\n"
"}";

static char unv_str_alarm_human_area_str[] = 
"{\n"
"\t\"Num\":%d,\n"
"\t\"RectangleAreasList\":[%s]\n"
"}";


static char unv_str_alarm_capability[] =
"{\r\n"
"\t\"MotionDetection\":{\r\n"
"\t\t\"IsSupportCfg\":1,\r\n"
"\t\t\"RectangleAreaNum\":4,\r\n"
"\t\t\"BlockWidth\":22,\r\n"
"\t\t\"BlockHeight\":18\r\n"
"\t},\r\n"
"\t\"TamperDetection\":{\r\n"
"\t\t\"IsSupportCfg\":1,\r\n"
"\t\t\"TamperRectangleAreaNum\":1\r\n"
"\t},\r\n"
"\t\"AudioDetection\":{\r\n"
"\t\t\"SupportCfg\":0\r\n"
"\t},\r\n"
"\t\"HumanShapeDetection\":{\r\n" 
"\t\t\"SupportCfg\":%d,\r\n"
"\t\t\"RectangleAreaNum\":%d\r\n"
"\t}\n"
"}";

static char unv_str_rect_areas_list[] = 
"{\n"
"\t\t\t\"ID\":\"%s\",\n"
"\t\t\t\"Enabled\":%d,\n"
"\t\t\t\"Sensitivity\":%d,\n"
"\t\t\t\"TargetSize\":%d,\n"
"\t\t\t\"Area\": {\n"
"\t\t\t\t\"TopLeft\":{\n"
"\t\t\t\t\t\"X\":%d,\n"
"\t\t\t\t\t\"Y\":%d\n"
"\t\t\t\t},\n"
"\t\t\t\t\"BottomRight\":{\n"
"\t\t\t\t\t\"X\":%d,\n"
"\t\t\t\t\t\"Y\":%d\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t}";

void get_rectangle_area_from_cfg(Polygon *pPolygonArea, int *x1, int *y1, int *x2, int *y2)
{
    if(pPolygonArea == NULL || x1 == NULL || y1 == NULL || x2== NULL || y2== NULL)
        return;

    int i = 0;
    int _x1 = 100;
    int _x2 = 0;
    int _y1 = 100;
    int _y2 = 0;

    for(i = 0; i < pPolygonArea->count && i < MAX_POLYGON_POINT_CNT; i++)
    {
        if(_x1 > pPolygonArea->points[i].x)
            _x1 = pPolygonArea->points[i].x;

        if(_x2 < pPolygonArea->points[i].x)
            _x2 = pPolygonArea->points[i].x;        

        if(_y1 > pPolygonArea->points[i].y)
            _y1 = pPolygonArea->points[i].y;

        if(_y2 < pPolygonArea->points[i].y)
            _y2 = pPolygonArea->points[i].y;            
    }

    *x1 = _x1*100;
    *y1 = _y1*100;
    *x2 = _x2*100;
    *y2 = _y2*100;
}


//-------------------------------------------------------------------------------------------
//                             unv  image  
//-------------------------------------------------------------------------------------------

int image_lampctrl_info_get(cJSON **p, int flag)
{
    cJSON *LampInfo = cJSON_CreateObject();
    if (flag == 0)
    {
        cJSON_AddNumberToObject(LampInfo, "LampType", 1);
    }
    else if(flag == 1)
    {
        cJSON_AddNumberToObject(LampInfo, "LampType", 2);
    }
    else if(flag == 2)
    {
        cJSON_AddNumberToObject(LampInfo, "LampType", 6);
    }
    cJSON_AddNumberToObject(LampInfo, "LampCtrlModeListNum", 4);

    cJSON *LampCtrlModeList = cJSON_CreateArray();
    cJSON *NUM1 = cJSON_CreateNumber(0);
    cJSON *NUM2 = cJSON_CreateNumber(4);
    cJSON *NUM3 = cJSON_CreateNumber(5);
    cJSON *NUM4 = cJSON_CreateNumber(7);
    cJSON_AddItemToArray(LampCtrlModeList, NUM1);
    cJSON_AddItemToArray(LampCtrlModeList, NUM2);
    cJSON_AddItemToArray(LampCtrlModeList, NUM3);
    cJSON_AddItemToArray(LampCtrlModeList, NUM4);

    cJSON_AddItemToObject(LampInfo, "LampCtrlModeList", LampCtrlModeList);

    cJSON_AddNumberToObject(LampInfo, "SupportNearLampCfg", 1);
    cJSON_AddNumberToObject(LampInfo, "SupportMiddleLampCfg", 0);
    cJSON_AddNumberToObject(LampInfo, "SupportFarLampCfg", 0);
    cJSON_AddNumberToObject(LampInfo, "SupportSuperFarLampCfg", 0);
    cJSON_AddNumberToObject(LampInfo, "SupportLaserAngleCfg", 0);

    *p = LampInfo;
    return 0;
}

int image_lampctrl_capability_get(cJSON **p)//灯光能力
{
    cJSON *LampCtrlCapabilitiesInfo = cJSON_CreateObject();
    if (LampCtrlCapabilitiesInfo == NULL)
    {
        __ERR("cJSON_CreateObject LampCtrlCapabilitiesInfo failed\n");
        return -1;
    }

    cJSON_AddNumberToObject(LampCtrlCapabilitiesInfo, "LampNum", 3);

    cJSON *LampInfos = cJSON_CreateArray();
    int i = 0;
    for (i = 0; i < 3; i++)
    {
        cJSON *LampInfo = NULL;
        image_lampctrl_info_get(&LampInfo, i);
        cJSON_AddItemToArray(LampInfos, LampInfo);
    }

    cJSON_AddItemToObject(LampCtrlCapabilitiesInfo, "LampInfos", LampInfos);
    
    *p = LampCtrlCapabilitiesInfo;
    return 0;
}

int unv_image_capability_get(cJSON **p)//图像能力集
{
    cJSON *Image_Capabilities = cJSON_CreateObject();
    if (Image_Capabilities == NULL)
    {
        __ERR("cJSON_CreateObject Image_Capabilities failed\n");
        return -1;
    }

    cJSON *Enhance = cJSON_CreateObject(); 

    const int anNum[4] = {0,1,2,3};
    cJSON *ImageRotationModeList = cJSON_CreateIntArray(anNum, 4);

    cJSON_AddNumberToObject(Enhance, "ImageRotationModeNum", 4);
    cJSON_AddItemToObject(Enhance, "ImageRotationModeList", ImageRotationModeList);
    cJSON_AddNumberToObject(Enhance, "IsSupportSharpness", 1);
    cJSON_AddNumberToObject(Enhance, "IsSupport2DNoiseReduce", 1);
    cJSON_AddNumberToObject(Enhance, "IsSupport3DNoiseReduce", 1);
    
    cJSON_AddNumberToObject(Image_Capabilities, "IsSupportCfg", 1);
    cJSON_AddItemToObject(Image_Capabilities, "Enhance", Enhance);
    
    cJSON *LampCtrl = NULL;    
    image_lampctrl_capability_get(&LampCtrl);

    cJSON_AddItemToObject(Image_Capabilities, "LampCtrl", LampCtrl);

    *p = Image_Capabilities;
    return 0;
}

int unv_image_lampctrl_get(cJSON **p)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *ImageLampctrl = cJSON_CreateObject();
    if (ImageLampctrl == NULL)
    {
        __ERR("cJSON_CreateObject ImageLampctrl failed\n");
        return -1;
    }

    if (pstVideoCapCfg->led_brightness_mode == LED_BRIGHTNESS_MODE_MANUAL &&
        pstVideoCapCfg->led_brightness_value == 0 )
        cJSON_AddNumberToObject(ImageLampctrl, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ImageLampctrl, "Enabled", 1);

    if (pstVideoCapCfg->led_mode == 0)
        cJSON_AddNumberToObject(ImageLampctrl, "Type", 2);
    else if (pstVideoCapCfg->led_mode == 1)
        cJSON_AddNumberToObject(ImageLampctrl, "Type", 1);
    else
        cJSON_AddNumberToObject(ImageLampctrl, "Type", 6);

    if (pstVideoCapCfg->ircut_mode == 0)
        cJSON_AddNumberToObject(ImageLampctrl, "Mode", 0);
    else if (pstVideoCapCfg->ircut_mode == 1)
        cJSON_AddNumberToObject(ImageLampctrl, "Mode", 4);
    else if (pstVideoCapCfg->ircut_mode == 2)
        cJSON_AddNumberToObject(ImageLampctrl, "Mode", 5);
    else if (pstVideoCapCfg->ircut_mode == 3)
        cJSON_AddNumberToObject(ImageLampctrl, "Mode", 7);
    else
        cJSON_AddNumberToObject(ImageLampctrl, "Mode", 7);
    
    cJSON_AddNumberToObject(ImageLampctrl, "NearLevel", pstVideoCapCfg->led_brightness_value * 10);

    *p = ImageLampctrl;
    return 0;
}

int unv_image_enhance_get(cJSON **p)//图像配置
{
    cJSON *ImageEnhance = cJSON_CreateObject();
    if (ImageEnhance == NULL)
    {
        __ERR("cJSON_CreateObject ImageEnhance failed\n");
        return -1;
    }

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    if (pstVideoCapCfg->led_mode == 0)
    {
        cJSON_AddNumberToObject(ImageEnhance, "SourceType", 0);
    }
    else if (pstVideoCapCfg->led_mode == 1)
    {
        cJSON_AddNumberToObject(ImageEnhance, "SourceType", 1);
    }

    cJSON_AddNumberToObject(ImageEnhance, "Brightness", pstVideoCapCfg->brightness);
    cJSON_AddNumberToObject(ImageEnhance, "Contrast", pstVideoCapCfg->contrast);
    cJSON_AddNumberToObject(ImageEnhance, "Saturation", pstVideoCapCfg->saturation);
    cJSON_AddNumberToObject(ImageEnhance, "Sharpness", pstVideoCapCfg->sharpness);

    if (pstVideoCapCfg->hflip == 0 && pstVideoCapCfg->vflip == 0)
    {
        cJSON_AddNumberToObject(ImageEnhance, "ImageRotation", 0);
    }
    else if (pstVideoCapCfg->hflip == 1 && pstVideoCapCfg->vflip == 0)
    {
        cJSON_AddNumberToObject(ImageEnhance, "ImageRotation", 1);
    }
    else if (pstVideoCapCfg->hflip == 0 && pstVideoCapCfg->vflip == 1)
    {
        cJSON_AddNumberToObject(ImageEnhance, "ImageRotation", 2);
    }
    else if (pstVideoCapCfg->hflip == 1 && pstVideoCapCfg->vflip == 1)
    {
        cJSON_AddNumberToObject(ImageEnhance, "ImageRotation", 3);
    }
    cJSON_AddNumberToObject(ImageEnhance, "2DNoiseReduce", 0);
    cJSON_AddNumberToObject(ImageEnhance, "3DNoiseReduce", 0);
    cJSON_AddNumberToObject(ImageEnhance, "NeedSetEnhanceInfo", 0);

    *p = ImageEnhance;
    return 0;
}

//-------------------------------------------------------------------------------------------
//                          media
//-------------------------------------------------------------------------------------------

int unv_media_audio_input_get(cJSON **p)
{
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAudioCfg = &pstMediaCfg->audioConfig;

    cJSON *AudioInputJson = cJSON_CreateObject();
    if(AudioInputJson == NULL)
    {
        __ERR("cJSON_CreateObject AudioInputJson failed\n");
        return -1;
    }
    
    if (pstAudioCfg->audioEncode.enable == 0)
        cJSON_AddNumberToObject(AudioInputJson, "IsMute", 1);
    else
        cJSON_AddNumberToObject(AudioInputJson, "IsMute", 0);
    
    cJSON_AddNumberToObject(AudioInputJson, "Type", 0);

    if (strcmp(pstAudioCfg->audioEncode.audioEncodeType.typeName, "G.711") == 0)
        cJSON_AddNumberToObject(AudioInputJson, "EncodeFormat", 2);
    else if (strcmp(pstAudioCfg->audioEncode.audioEncodeType.typeName, "G.711A") == 0)
        cJSON_AddNumberToObject(AudioInputJson, "EncodeFormat", 1);
    else if (strcmp(pstAudioCfg->audioEncode.audioEncodeType.typeName, "AAC") == 0)
        cJSON_AddNumberToObject(AudioInputJson, "EncodeFormat", 6);
    else
        cJSON_AddNumberToObject(AudioInputJson, "EncodeFormat", 2);
    
    if(pstAudioCfg->audioEncode.sampleRate == 16000)
        cJSON_AddNumberToObject(AudioInputJson, "SampleRate", 1);
    else
        cJSON_AddNumberToObject(AudioInputJson, "SampleRate", 0);

    cJSON_AddNumberToObject(AudioInputJson, "InputGain", (int)(round(pstAudioCfg->audioCapture.volume_capture * 2.55)));

    cJSON *NoiseReduction = cJSON_CreateObject();
    cJSON_AddNumberToObject(NoiseReduction, "Enabled", 0);
    cJSON_AddNumberToObject(NoiseReduction, "Mode", 0);
    cJSON_AddNumberToObject(NoiseReduction, "Strength", 0);
    cJSON_AddItemToObject(AudioInputJson, "NoiseReduction", NoiseReduction);

    cJSON_AddNumberToObject(AudioInputJson, "AudioInputNum", 1);

    if (1)
    {
        cJSON *AudioInputList = cJSON_CreateArray();
        cJSON *AudioInput = cJSON_CreateObject();
        cJSON_AddNumberToObject(AudioInput, "ID", 1);

        if(pstAudioCfg->audioEncode.enable == 1)
            cJSON_AddNumberToObject(AudioInput, "Enabled", 1);
        else
            cJSON_AddNumberToObject(AudioInput, "Enabled", 0);
        cJSON_AddNumberToObject(AudioInput, "Mode", 1);
        cJSON_AddItemToArray(AudioInputList, AudioInput);
        cJSON_AddItemToObject(AudioInputJson, "AudioInputList", AudioInputList);
    }
    cJSON_AddNumberToObject(AudioInputJson, "SerialIInputNum", 0);

    cJSON *SerialIInputList = cJSON_CreateArray();
    cJSON_AddItemToObject(AudioInputJson, "SerialIInputList", SerialIInputList);

    *p = AudioInputJson;
    return 0;
}

int unv_media_audio_output_get(cJSON **p)
{
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig *pstAudioCfg = &pstMediaCfg->audioConfig;

    cJSON *AudioOutputJson = cJSON_CreateObject();
    if(AudioOutputJson == NULL)
    {
        __ERR("cJSON_CreateObject AudioOutputJson failed\n");
        return -1;
    }

    int volume_play = (int)(round(pstAudioCfg->audioCapture.volume_play * 2.55));
    cJSON_AddNumberToObject(AudioOutputJson, "Type", 0);
    cJSON_AddNumberToObject(AudioOutputJson, "Gain", volume_play);
    cJSON_AddNumberToObject(AudioOutputJson, "AlarmGain", volume_play);

    *p = AudioOutputJson;
    return 0;
}


//-------------------------------------------------------------------------------------------
//                             unv  system  
//-------------------------------------------------------------------------------------------

int unv_system_capability_get(cJSON **p)
{
    cJSON *SystemCapInfo = cJSON_CreateObject();
    cJSON *AudioFile = cJSON_CreateObject();
    cJSON *LightAlarmCapabilities = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(SystemCapInfo, "SupportedChannelNums", 2);

    cJSON_AddNumberToObject(AudioFile, "MaxNum", 10);
    cJSON_AddNumberToObject(AudioFile, "MaxSize", 0);
    cJSON_AddNumberToObject(AudioFile, "FormatNum", 0);
    cJSON_AddItemToObject(SystemCapInfo, "AudioFile", AudioFile);

    cJSON_AddNumberToObject(SystemCapInfo, "IsSupportedAudioAlarm", 1);
    cJSON_AddNumberToObject(SystemCapInfo, "IsSupportedLightAlarm", 1);
    cJSON_AddNumberToObject(SystemCapInfo, "SupportAllAlarmSubscription", 1);
    cJSON_AddNumberToObject(LightAlarmCapabilities, "MaxInterval", 5);
    cJSON_AddNumberToObject(LightAlarmCapabilities, "MinInterval", 2);
    cJSON_AddItemToObject(SystemCapInfo, "LightAlarmCapabilities", LightAlarmCapabilities);

    *p = SystemCapInfo;
    return 0;
}

int unv_system_audiofile_get(cJSON **p)
{
    cJSON *SystemAudioFile = cJSON_CreateObject();
    if (SystemAudioFile == NULL)
    {
        __ERR("cJSON_CreateObject SystemAudioFile failed\n");
        return -1;
    }

    AudioFileList *pstAudioFileList = http_audio_file_list_get();
    int Num = pstAudioFileList->Num;
    if (Num > 0)
    {
        int i = 0;
        cJSON_AddNumberToObject(SystemAudioFile, "Num", Num);
        cJSON *FileInfoList = cJSON_CreateArray();
        for (i = 0; i < Num; i++)
        {
            cJSON *AudioFileInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(AudioFileInfo, "ID", pstAudioFileList->Item[i].ID);
            cJSON_AddNumberToObject(AudioFileInfo, "Status", 0);
            cJSON_AddNumberToObject(AudioFileInfo, "InstallType", 0);
            cJSON_AddStringToObject(AudioFileInfo, "FileName", pstAudioFileList->Item[i].file_onlyname);
            
            cJSON_AddItemToArray(FileInfoList, AudioFileInfo);
        }
        cJSON_AddItemToObject(SystemAudioFile, "FileInfoList", FileInfoList);
    }
    else
    {
        cJSON_AddNumberToObject(SystemAudioFile, "Num", 0);
    }

    *p = SystemAudioFile;
    return 0;
}

int unv_system_photoserver_get(cJSON **p)
{
    MediaStreamConfig *pstMediaStreamCfg = (MediaStreamConfig *)getMediaStreamConfig();

    cJSON *PhotoServerCfg = cJSON_CreateObject();
    cJSON *ServerAddr = cJSON_CreateObject();

    cJSON_AddNumberToObject(PhotoServerCfg, "Protocol", 1);

    NET_IPV4 ip = {0};
    char ip_addr[64] = "";
    ip.int32 = get_my_ipaddr();     
    snprintf(ip_addr, sizeof(ip_addr), "%d.%d.%d.%d", ip.str[0], ip.str[1], ip.str[2], ip.str[3]);

    cJSON_AddStringToObject(ServerAddr, "Address", ip_addr);
    cJSON_AddNumberToObject(ServerAddr, "Port", pstMediaStreamCfg->webConfig.webPort);
    cJSON_AddItemToObject(PhotoServerCfg, "ServerAddr", ServerAddr);

    *p = PhotoServerCfg;
    return 0;
}

int unv_media_audio_capability_get(cJSON **p)
{
    cJSON *MediaAudioCapInfo = cJSON_CreateObject();
    if (MediaAudioCapInfo == NULL)
    {
        __ERR("cJSON_CreateObject MediaAudioCapInfo failed\n");
        return -1;
    }

    if (1)
    {
        int AudioInNum = 0;
        cJSON *AudioInModeInfoList = cJSON_CreateArray();
        if (1)
        {
            cJSON *AudioModeInfo  = cJSON_CreateObject();
            cJSON_AddNumberToObject(AudioModeInfo, "Channel", 1);
            cJSON_AddNumberToObject(AudioModeInfo, "ModeNum", 1);

            cJSON *ModeList = cJSON_CreateArray();
            cJSON *NUM = cJSON_CreateNumber(1);
            cJSON_AddItemToArray(ModeList, NUM);
            cJSON_AddItemToObject(AudioModeInfo, "ModeList", ModeList);
            cJSON_AddItemToArray(AudioInModeInfoList, AudioModeInfo);
            AudioInNum ++ ;
        }
        cJSON_AddNumberToObject(MediaAudioCapInfo, "AudioInNum", AudioInNum);
        cJSON_AddItemToObject(MediaAudioCapInfo, "AudioInModeInfoList", AudioInModeInfoList);
    }

    if (1)
    {
        int AudioInEncodeFormatNum = 0;
        cJSON *AudioInEncodeFormatInfoList = cJSON_CreateArray();
        if (1)
        {
            cJSON *EncodeFormatInfo  = cJSON_CreateObject();
            cJSON_AddNumberToObject(EncodeFormatInfo, "Type", 1);
            cJSON_AddNumberToObject(EncodeFormatInfo, "Num", 2);

            cJSON *SampleList = cJSON_CreateArray();
            cJSON *NUM1 = cJSON_CreateNumber(0);
            cJSON_AddItemToArray(SampleList, NUM1);
            cJSON *NUM2 = cJSON_CreateNumber(1);
            cJSON_AddItemToArray(SampleList, NUM2);
            cJSON_AddItemToObject(EncodeFormatInfo, "SampleList", SampleList);

            cJSON_AddItemToArray(AudioInEncodeFormatInfoList, EncodeFormatInfo);
            AudioInEncodeFormatNum ++;
        }

        if (1)
        {
            cJSON *EncodeFormatInfo  = cJSON_CreateObject();
            cJSON_AddNumberToObject(EncodeFormatInfo, "Type", 2);
            cJSON_AddNumberToObject(EncodeFormatInfo, "Num", 2);

            cJSON *SampleList = cJSON_CreateArray();
            cJSON *NUM1 = cJSON_CreateNumber(0);
            cJSON_AddItemToArray(SampleList, NUM1);
            cJSON *NUM2 = cJSON_CreateNumber(1);
            cJSON_AddItemToArray(SampleList, NUM2);
            cJSON_AddItemToObject(EncodeFormatInfo, "SampleList", SampleList);

            cJSON_AddItemToArray(AudioInEncodeFormatInfoList, EncodeFormatInfo);
            AudioInEncodeFormatNum ++;
        }

        if (1)
        {
            cJSON *EncodeFormatInfo  = cJSON_CreateObject();
            cJSON_AddNumberToObject(EncodeFormatInfo, "Type", 6);
            cJSON_AddNumberToObject(EncodeFormatInfo, "Num", 1);

            cJSON *SampleList = cJSON_CreateArray();
            cJSON *NUM = cJSON_CreateNumber(1);
            cJSON_AddItemToArray(SampleList, NUM);
            cJSON_AddItemToObject(EncodeFormatInfo, "SampleList", SampleList);

            cJSON_AddItemToArray(AudioInEncodeFormatInfoList, EncodeFormatInfo);
            AudioInEncodeFormatNum ++;
        }
        cJSON_AddNumberToObject(MediaAudioCapInfo, "AudioInEncodeFormatNum", AudioInEncodeFormatNum);
        cJSON_AddItemToObject(MediaAudioCapInfo, "AudioInEncodeFormatInfoList", AudioInEncodeFormatInfoList);
    }

    if (1)
    {
        cJSON_AddNumberToObject(MediaAudioCapInfo, "SerialInNum", 0);
        cJSON *SerialInModeInfoList = cJSON_CreateArray();
        cJSON_AddItemToObject(MediaAudioCapInfo, "SerialInModeInfoList", SerialInModeInfoList);
    }

    if (1)
    {
        cJSON_AddNumberToObject(MediaAudioCapInfo, "SerialInEncodeFormatNum", 0);
        cJSON *SerialInEncodeFormatInfoList = cJSON_CreateArray();
        cJSON_AddItemToObject(MediaAudioCapInfo, "SerialInEncodeFormatInfoList", SerialInEncodeFormatInfoList);
    }

    if (1)
    {
        cJSON_AddNumberToObject(MediaAudioCapInfo, "AudioOutTypeNum", 1);

        cJSON *AudioOutTypeInfoList = cJSON_CreateArray();
        cJSON *NUM = cJSON_CreateNumber(0);
        cJSON_AddItemToArray(AudioOutTypeInfoList, NUM);

        cJSON_AddItemToObject(MediaAudioCapInfo, "AudioOutTypeInfoList", AudioOutTypeInfoList);
        cJSON_AddNumberToObject(MediaAudioCapInfo, "SupportAudioOutGain", 1);
        cJSON_AddNumberToObject(MediaAudioCapInfo, "SupportAlarmAudioOutGain", 1);
    }
    
    *p = MediaAudioCapInfo;
    return 0;
}

//-------------------------------------------------------------------------------------------
//                          smart vehicle
//-------------------------------------------------------------------------------------------

int smart_vehicle_capability_info_get(int car_enable, cJSON **p)
{
    cJSON * VehicleDetectionCapInfo = cJSON_CreateObject();
    cJSON *LinkagePlanCfg           = cJSON_CreateObject();

    if (car_enable == 1)
    {
        cJSON_AddNumberToObject(VehicleDetectionCapInfo, "SupportCfg", 1);
        cJSON_AddNumberToObject(VehicleDetectionCapInfo, "SupportPointNum", 2);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 1);
        cJSON_AddItemToObject(VehicleDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    }
    else
    {
        cJSON_AddNumberToObject(VehicleDetectionCapInfo, "SupportCfg", 0);
        cJSON_AddNumberToObject(VehicleDetectionCapInfo, "SupportPointNum", 0);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 0);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 0);
        cJSON_AddItemToObject(VehicleDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    }

    *p = VehicleDetectionCapInfo;
    return 0;
}

//获取车型告警规则信息
int unv_smart_vehicle_rule_get(cJSON **p)
{
    cJSON *VehicleDetectRuleInfo = cJSON_CreateObject();
    if (VehicleDetectRuleInfo == NULL)
    {
        __ERR("cJSON_CreateObject VehicleDetectRuleInfo failed\n");
        return -1;
    }

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    char type_buf[33] = {0};
    type_buf[32] = '\0';
    ToBin(pstPdAlarm->type, type_buf);

    if (type_buf[31 - 0] == 0x31)
    {
        cJSON_AddNumberToObject(VehicleDetectRuleInfo, "Enabled", 1);
    }
    else
    {
        cJSON_AddNumberToObject(VehicleDetectRuleInfo, "Enabled", 0);
    }

    *p = VehicleDetectRuleInfo;
    return 0;
}

//获取车型单个区域运动检测信息
int unv_smart_vehicle_areas0_get(cJSON **p)
{
    cJSON *p1           = cJSON_CreateObject();
    cJSON *PointList    = cJSON_CreateArray();

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    int i = 0;
    int ID = 0;
    int PointNum = pstPdAlarm->polygonArea.count;

    cJSON_AddNumberToObject(p1, "ID", ID);

    if(pstPdAlarm->polygonArea.count > 4)
    {
        cJSON_AddNumberToObject(p1, "Enabled", 1);
        if (pstPdAlarm->threshold == 75 || pstPdAlarm->threshold > 10)
        {
            pstPdAlarm->threshold = 0;
        }
        cJSON_AddNumberToObject(p1, "Sensitivity", (pstPdAlarm->sensitivity * 10) + pstPdAlarm->threshold);
        cJSON_AddNumberToObject(p1, "PointNum", PointNum);

        if (PointNum > 0)
        {
            for(i = 0; i < PointNum; i++)
            {
                cJSON *XY = cJSON_CreateObject(); //free 4
                cJSON_AddNumberToObject(XY, "X", (pstPdAlarm->polygonArea.points[i].x)*100);
                cJSON_AddNumberToObject(XY, "Y", (pstPdAlarm->polygonArea.points[i].y)*100);
                cJSON_AddItemToArray(PointList, XY);    
            }
        }
    }

    if(pstPdAlarm->polygonArea.count == 4)
    {
        int x1, y1, x2, y2;
        get_rectangle_area_from_cfg(&(pstPdAlarm->polygonArea), &x1, &y1, &x2, &y2);

        cJSON_AddNumberToObject(p1, "Enabled", 1);
        if (pstPdAlarm->threshold == 75 || pstPdAlarm->threshold > 10)
        {
            pstPdAlarm->threshold = 0;
        }
        cJSON_AddNumberToObject(p1, "Sensitivity", (pstPdAlarm->sensitivity * 10) + pstPdAlarm->threshold);
        cJSON_AddNumberToObject(p1, "PointNum", 2);

        cJSON *XY1 = cJSON_CreateObject(); //free 5
        cJSON_AddNumberToObject(XY1, "X", x1);
        cJSON_AddNumberToObject(XY1, "Y", y1);
        cJSON_AddItemToArray(PointList, XY1);        

        cJSON *XY2 = cJSON_CreateObject(); //free 6
        cJSON_AddNumberToObject(XY2, "X", x2);
        cJSON_AddNumberToObject(XY2, "Y", y2);
        cJSON_AddItemToArray(PointList, XY2);
    }
    else if(pstPdAlarm->polygonArea.count == 3)
    {
        cJSON_AddNumberToObject(p1, "Enabled", 1);
        if (pstPdAlarm->threshold == 75 || pstPdAlarm->threshold > 10)
        {
            pstPdAlarm->threshold = 0;
        }
        cJSON_AddNumberToObject(p1, "Sensitivity", (pstPdAlarm->sensitivity * 10) + pstPdAlarm->threshold);
        cJSON_AddNumberToObject(p1, "PointNum", 3);
        
        PointNum = pstPdAlarm->polygonArea.count;
        if (PointNum > 0)
        {
            for(i = 0; i < PointNum; i++)
            {
                cJSON *XY = cJSON_CreateObject();
                cJSON_AddNumberToObject(XY, "X", (pstPdAlarm->polygonArea.points[i].x)*100);
                cJSON_AddNumberToObject(XY, "Y", (pstPdAlarm->polygonArea.points[i].y)*100);
                cJSON_AddItemToArray(PointList, XY);    
            }
        }
    }
    else
    {
        cJSON_AddNumberToObject(p1, "Enabled", 0);
        if (pstPdAlarm->threshold == 75 || pstPdAlarm->threshold > 10)
        {
            pstPdAlarm->threshold = 0;
        }

        cJSON_AddNumberToObject(p1, "Sensitivity", (pstPdAlarm->sensitivity * 10) + pstPdAlarm->threshold);
        cJSON_AddNumberToObject(p1, "PointNum", 2);
        
        PointNum = pstPdAlarm->polygonArea.count;
        if (PointNum > 0)
        {
            for(i = 0; i < PointNum; i++)
            {
                cJSON *XY = cJSON_CreateObject();
                cJSON_AddNumberToObject(XY, "X", (pstPdAlarm->polygonArea.points[i].x)*100);
                cJSON_AddNumberToObject(XY, "Y", (pstPdAlarm->polygonArea.points[i].y)*100);
                
                cJSON_AddItemToArray(PointList, XY);    
            }
        }
    }    

    cJSON_AddItemToObject(p1, "PointList", PointList);

    *p = p1;
    return 0;
}

//获取车型多区域运动检测信息
int unv_smart_vehicle_areas_get(cJSON **p)
{ 
    cJSON *VehicleDetectAreaInfoList = cJSON_CreateObject();
    if (VehicleDetectAreaInfoList == NULL)
    {
        __ERR("cJSON_CreateObject VehicleDetectAreaInfoList failed\n");
        return -1;
    }

    int i = 0;
    int Num = 1;
    cJSON *PolygonInfoList = cJSON_CreateArray(); 

    cJSON_AddNumberToObject(VehicleDetectAreaInfoList, "Num", Num);
    for (i = 0; i < Num; i ++)
    {
        cJSON *VehicleDetectPolygonInfo = NULL;
        unv_smart_vehicle_areas0_get(&VehicleDetectPolygonInfo);
        cJSON_AddItemToArray(PolygonInfoList, VehicleDetectPolygonInfo);    
    }
    cJSON_AddItemToObject(VehicleDetectAreaInfoList, "PolygonInfoList", PolygonInfoList);

    *p = VehicleDetectAreaInfoList;
    return 0;
}


//-------------------------------------------------------------------------------------------
//                          alarm human
//-------------------------------------------------------------------------------------------

void alarm_human_enable_str_get(char *str, int len, int enable)
{
    snprintf(str, len, unv_str_alarm_human_enable_str, enable);   
}

//获取布防计划
int unv_alarm_human_week_plan_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    char str[33] = {0};
    char str1[24] = {0};
    str[32] = '\0';

    int day = 0;
    int time = 0;

    /*1.初始化week*/ 
    cJSON *Week_Json = cJSON_CreateObject();
    cJSON *Days_arry = cJSON_CreateArray();

    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);
    int Week_Num = 0;
    int Num = 0;

    for (day = 1; day <= 7; day++)
    {
        if (day == 7)
        {
            ToBin(pstPdAlarm->timeSpan.workday[0], str);
        }
        else
        {
            ToBin(pstPdAlarm->timeSpan.workday[day], str);
        }

        strcpy(str1, str + 8);
        if (strcmp(str1, "000000000000000000000000"))
        {
            /*1.初始化DAY*/ 
            cJSON *Day_Json = cJSON_CreateObject();
            cJSON *TimeSectionInfos_arry = cJSON_CreateArray();
            cJSON_AddNumberToObject(Day_Json, "ID", day);
            Num = 0;
            time = 0;

            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {
                    char Begin[16] = {0};
                    char End[16]   = {0};
                    char Begint_type[12] = "%2d:00:00";
                    char End_type[12]    = "%2d:00:00";

                    cJSON *Time_Json=cJSON_CreateObject(); // free 7
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");
                    Num++;

                    snprintf(Begin, sizeof(Begin), Begint_type, time);
                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);
                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            snprintf(End, sizeof(End), End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            snprintf(End, sizeof(End), End_type, time + 1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }                
                        else
                        {
                            time++;
                        }
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;    
            cJSON_AddNumberToObject(Day_Json, "Num", Num);

            /*3.结构体嵌套填充*/
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    /*3.结构体嵌套填充*/
    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);
    *p = Week_Json;

    return 0;
}

int unv_alarm_human_rule_get(cJSON **p)
{
    int buf_len = 128;
    char *buf = (char *)anj_mw_malloc(buf_len);
    if (buf == NULL)
    {
        __ERR("buf malloc failed\n");
    }

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdConfig = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    if((pstPdConfig->enable) && (pstPdConfig->type & (1<<4)))
        alarm_human_enable_str_get(buf, buf_len, 1);
    else
        alarm_human_enable_str_get(buf, buf_len, 0);
    
    cJSON *jsonbuf = cJSON_Parse(buf);

    anj_mw_free(buf);
    buf = NULL;

    *p = jsonbuf;
    return 0;
}

int unv_alarm_human_areas_get(char *pChn, cJSON **p)
{
    int buf_len = 1024;
    char *buf = (char *)anj_mw_malloc(sizeof(char)*buf_len);
    if (buf == NULL)
    {
        return -1;
    }

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdConfig = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    if(pstPdConfig->polygonArea.count > 2)
    {
        int AreasListStrlength = 0;
        char *AreasListstr = NULL;

        int sensitivity = 0;

        int x1, y1, x2, y2;

        AreasListStrlength = strlen(unv_str_rect_areas_list) + 128;

        AreasListstr = (char *)anj_mw_malloc(sizeof(char)*AreasListStrlength);
        if(AreasListstr == NULL)
        {
            return -1;
        }

        sensitivity = pstPdConfig->sensitivity * 10;
        if (pstPdConfig->threshold == 75 || pstPdConfig->threshold > 10)
        {
            pstPdConfig->threshold = 0;
        }

        sensitivity += pstPdConfig->threshold;
        get_rectangle_area_from_cfg(&(pstPdConfig->polygonArea), &x1, &y1, &x2, &y2);

        snprintf(AreasListstr, AreasListStrlength, unv_str_rect_areas_list, pChn, 1, sensitivity, 4, x1, y1, x2, y2);
        snprintf(buf, buf_len, unv_str_alarm_human_area_str, 1, AreasListstr);

        anj_mw_free(AreasListstr);
        AreasListstr = NULL;
    }
    else
    {
        snprintf(buf, buf_len, unv_str_alarm_human_area_str, 0, "");
    }

    cJSON *jsonbuf = cJSON_Parse(buf);

    anj_mw_free(buf);
    buf = NULL;

    *p = jsonbuf;
    return 0;
}

//-------------------------------------------------------------------------------------------
//                          smart intrusion
//-------------------------------------------------------------------------------------------

int smart_intrusion_capability_info_get(int region_enable, cJSON **p)
{
    cJSON *IntrusionDetectionCapInfo = cJSON_CreateObject();
    cJSON *LinkagePlanCfg = cJSON_CreateObject();
    
    if (region_enable)
    {
        cJSON_AddNumberToObject(IntrusionDetectionCapInfo, "SupportCfg", 1);
        cJSON_AddNumberToObject(IntrusionDetectionCapInfo, "Mode", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 1);
    }
    else
    {
        cJSON_AddNumberToObject(IntrusionDetectionCapInfo, "SupportCfg", 0);
        cJSON_AddNumberToObject(IntrusionDetectionCapInfo, "Mode", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 0);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 0);
    }

    cJSON_AddItemToObject(IntrusionDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    *p = IntrusionDetectionCapInfo;
    return 0;
}

int smart_intrusion_link_audio_alarm_info_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p, int flag)
{
    int i = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapCfg = &pstMediaCfg->audioConfig.audioCapture; 

    cJSON *ActParam = cJSON_CreateObject();

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    float vol_val = (float)pstAudioCapCfg->volume_play;
    vol_val = vol_val / 100 * 255;
    cJSON_AddNumberToObject(ActParam, "AudioVolume", (int)vol_val);

    AudioFileList *pstAudioFileList = http_audio_file_list_get();
    for (i = 0; i < pstAudioFileList->Num; i++)
    {
        int len = strlen(pstItemAlarm->alarmAction.audioAction.filename);
        if(strncmp(pstAudioFileList->Item[i].file_pathname, pstItemAlarm->alarmAction.audioAction.filename, len) == 0)
            cJSON_AddNumberToObject(ActParam, "AudioFileID", pstAudioFileList->Item[i].ID);
    }

    if (pstItemAlarm->alarmAction.audioAction.times >= 0 && pstItemAlarm->alarmAction.audioAction.times < 9)
        cJSON_AddNumberToObject(ActParam, "WarnCount", pstItemAlarm->alarmAction.audioAction.times);
    else
        cJSON_AddNumberToObject(ActParam, "WarnCount", 9);

    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *AudioActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(AudioActParamInfo, "ActID", UNV_ACT_ID_AUDIO_ALARM);
        cJSON_AddItemToObject(AudioActParamInfo, "ActParam", ActParam);
        *p = AudioActParamInfo;
    }

    return 0;
}

int smart_intrusion_link_audio_day_plan_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    int enable_flag = pstItemAlarm->alarmAction.audioAction.enable.enable_flag;

    cJSON *AudioCustomDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
            
            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.audioAction.enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
 
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.sec);
                
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }

            for (i = timespan_num; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
        else
        {
            for (i = 0; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
                
                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(AudioCustomDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = AudioCustomDayPlanInfo;
    return 0;
}

int smart_intrusion_link_audio_plan_info_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *AudioLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *AudioCustomInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 1);
    
    cJSON_AddNumberToObject(AudioLinkagePlanInfo, "ActID", 28);
    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddNumberToObject(AudioCustomInfo, "Num", 7);
    
    for (i = 1; i <= 7; i++)
    {
        cJSON *AudioCustomDayPlanInfo = NULL;
        smart_intrusion_link_audio_day_plan_get(pstItemAlarm, &AudioCustomDayPlanInfo, i);
        cJSON_AddItemToArray(Days, AudioCustomDayPlanInfo);
    }

    cJSON_AddItemToObject(AudioCustomInfo, "Days", Days);
    cJSON_AddItemToObject(ActParam, "AudioCustomInfo", AudioCustomInfo);
    cJSON_AddItemToObject(AudioLinkagePlanInfo, "ActParam", ActParam);

    *p = AudioLinkagePlanInfo;
    return 0;
}

int smart_intrusion_link_brled_light_alarm_get(VideoRegionAiAlarm *pItemAlarm, cJSON **p, int flag)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *ActParam = cJSON_CreateObject();

    if (pItemAlarm->alarmAction.alarm_led_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    cJSON_AddNumberToObject(ActParam, "Interval", 5);
    cJSON_AddNumberToObject(ActParam, "Luminance", pstVideoCapCfg->led_brightness_value * 10);

    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *LightActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(LightActParamInfo, "ActID", UNV_ACT_ID_LIGHT_ALARM);
        cJSON_AddItemToObject(LightActParamInfo, "ActParam", ActParam);
        *p = LightActParamInfo;
    }

    return 0;
}

int smart_intrusion_link_light_alarm_info_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p, int flag)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;
    
    cJSON *ActParam = cJSON_CreateObject();
    
    if (pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    cJSON_AddNumberToObject(ActParam, "Interval", 5);
    cJSON_AddNumberToObject(ActParam, "Luminance", pstVideoCapCfg->led_brightness_value * 10);
    
    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *LightActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(LightActParamInfo, "ActID", 25);
        cJSON_AddItemToObject(LightActParamInfo, "ActParam", ActParam);
        *p = LightActParamInfo;
    }

    return 0;
}

int smart_intrusion_link_brled_light_day_plan_get(VideoRegionAiAlarm *pItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    //int Num_real = pItemAlarm->alarmAction.alarm_led_enable.timespan_num;
    int enable_flag = pItemAlarm->alarmAction.alarm_led_enable.enable_flag;

    cJSON *LightDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(LightDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(LightDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pItemAlarm->alarmAction.alarm_led_enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.hour,
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.minute,
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.hour,
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.minute,
                    pItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.sec);

                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
        else
        {
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(LightDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = LightDayPlanInfo;
    return 0;
}

int smart_intrusion_link_light_day_plan_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;

    //int Num_real = pstItemAlarm->alarmAction.light_twinkle_enable.timespan_num;
    int enable_flag = pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag;

    cJSON *LightDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(LightDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(LightDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.light_twinkle_enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);

                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.sec);

                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
        else
        {
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(LightDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = LightDayPlanInfo;
    return 0;
}

int smart_intrusion_link_brled_light_plan_info_get(VideoRegionAiAlarm *pItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *LightLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *LightCustomInfo = cJSON_CreateObject();
    cJSON *LightWeekPlanInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pItemAlarm->alarmAction.alarm_led_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(LightLinkagePlanInfo, "ActID", UNV_ACT_ID_LIGHT_PLAN);
    cJSON_AddNumberToObject(LightWeekPlanInfo, "Num", 7);

    for (i = 1; i <= 7; i++)
    {
        cJSON *LightDayPlanInfo = NULL;
        smart_intrusion_link_brled_light_day_plan_get(pItemAlarm, &LightDayPlanInfo, i);
        cJSON_AddItemToArray(Days, LightDayPlanInfo);
    }

    cJSON_AddItemToObject(LightWeekPlanInfo, "Days", Days);

    cJSON *LightActParamInfo = NULL;
    smart_intrusion_link_brled_light_alarm_get(pItemAlarm, &LightActParamInfo, 1);

    cJSON_AddItemToObject(LightCustomInfo, "LightActParamInfo", LightActParamInfo);
    cJSON_AddItemToObject(LightCustomInfo, "LightWeekPlanInfo", LightWeekPlanInfo);

    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddItemToObject(ActParam, "LightCustomInfo", LightCustomInfo);

    cJSON_AddItemToObject(LightLinkagePlanInfo, "ActParam", ActParam);

    *p = LightLinkagePlanInfo;
    return 0;
}

int smart_intrusion_link_light_plan_info_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *LightLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *LightCustomInfo = cJSON_CreateObject();
    cJSON *LightWeekPlanInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(LightLinkagePlanInfo, "ActID", UNV_ACT_ID_LIGHT_PLAN);
    cJSON_AddNumberToObject(LightWeekPlanInfo, "Num", 7);

    for (i = 1; i <= 7; i++)
    {
        cJSON *LightDayPlanInfo = NULL;
        smart_intrusion_link_light_day_plan_get(pstItemAlarm, &LightDayPlanInfo, i);
        cJSON_AddItemToArray(Days, LightDayPlanInfo);
    }

    cJSON_AddItemToObject(LightWeekPlanInfo, "Days", Days);

    cJSON *LightActParamInfo = NULL;
    smart_intrusion_link_light_alarm_info_get(pstItemAlarm, &LightActParamInfo, 1);

    cJSON_AddItemToObject(LightCustomInfo, "LightActParamInfo", LightActParamInfo);
    cJSON_AddItemToObject(LightCustomInfo, "LightWeekPlanInfo", LightWeekPlanInfo);

    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddItemToObject(ActParam, "LightCustomInfo", LightCustomInfo);
 
    cJSON_AddItemToObject(LightLinkagePlanInfo, "ActParam", ActParam);

    *p = LightLinkagePlanInfo;
    return 0;
}

int smart_intrusion_target_info_get(VideoRegionAiAlarm *pstItemAlarm, cJSON **p, int type, int area_index)
{
    cJSON *DetectTargetInfo = cJSON_CreateObject();
    cJSON *ObjectMaxSize = cJSON_CreateObject();
    cJSON *ObjectMinSize = cJSON_CreateObject();

    int Enabled = 0;
    char type_buf[33] = {0};/*人车型是否使能*/
    type_buf[32] = '\0';
    ToBin(pstItemAlarm->data[area_index].type_filter, type_buf);

    if (type == 2)//人2非1车0
    {
        if(type_buf[31 - 4] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 2);
    }
    else if(type == 1)//人2非1车0
    {
        if((type_buf[31-1] == 0x31) || (type_buf[31-2] == 0x31) || (type_buf[31-3] == 0x31) || (type_buf[31-6] == 0x31))
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 1);
    }
    else if(type == 0)//人2非1车0
    {
        if(type_buf[31-0] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 0);
    }

    cJSON_AddNumberToObject(ObjectMaxSize, "Width", 9999);
    cJSON_AddNumberToObject(ObjectMaxSize, "Height", 9999);
    cJSON_AddNumberToObject(ObjectMinSize, "Width", 0);
    cJSON_AddNumberToObject(ObjectMinSize, "Height", 0);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMaxSize", ObjectMaxSize);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMinSize", ObjectMinSize);

    *p = DetectTargetInfo;
    return 0;
}


int unv_smart_intrusion_one_area_get(cJSON **p, int area_index, int index)
{
    cJSON *p1               = cJSON_CreateObject();
    cJSON *DetectTargetList = cJSON_CreateArray();

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[chn];

    int i = 0;
    int PointNum = pstItemAlarm->polygonArea.count;

    cJSON_AddNumberToObject(p1, "ID", index);
    if (pstItemAlarm->data[area_index].enable && (PointNum >= 3 && PointNum <= 6)  && index == 0)
    {
        cJSON_AddNumberToObject(p1, "Enabled", 1);
    }
    else
    {
        cJSON_AddNumberToObject(p1, "Enabled", 0);
    }
    
    cJSON_AddNumberToObject(p1, "Sensitivity", pstItemAlarm->data[area_index].sensitivity);
    if (area_index == 0)
    {
        if (pstItemAlarm->data[area_index].stayseconds < 1)
            cJSON_AddNumberToObject(p1, "TimeThreshold", 1);
        else if (pstItemAlarm->data[area_index].stayseconds > 10)
            cJSON_AddNumberToObject(p1, "TimeThreshold", 10);
        else
            cJSON_AddNumberToObject(p1, "TimeThreshold", pstItemAlarm->data[area_index].stayseconds);
        
        cJSON_AddNumberToObject(p1, "Percentage", 1);
    }

    if((PointNum >= 3 && PointNum <= 6)  && index == 0)
    {
        cJSON *PointList = cJSON_CreateArray();
        cJSON_AddNumberToObject(p1, "PointNum", PointNum);

        for(i = 0; i < PointNum; i++)
        {
            cJSON *XY = cJSON_CreateObject();
            cJSON_AddNumberToObject(XY, "X", (pstItemAlarm->polygonArea.points[i].x)*100);
            cJSON_AddNumberToObject(XY, "Y", (pstItemAlarm->polygonArea.points[i].y)*100);
            cJSON_AddItemToArray(PointList, XY);    
        }

        cJSON_AddItemToObject(p1, "PointList", PointList);
    }
    else
    {
        PointNum = 4;
        cJSON *PointList = cJSON_CreateArray();
        cJSON_AddNumberToObject(p1, "PointNum", PointNum);
        
        cJSON *XY1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(XY1, "X", 0);
        cJSON_AddNumberToObject(XY1, "Y", 0);
        cJSON_AddItemToArray(PointList, XY1);

        cJSON *XY2 = cJSON_CreateObject();
        cJSON_AddNumberToObject(XY2, "X", 9999);
        cJSON_AddNumberToObject(XY2, "Y", 0);
        cJSON_AddItemToArray(PointList, XY2);

        cJSON *XY3 = cJSON_CreateObject();
        cJSON_AddNumberToObject(XY3, "X", 9999);
        cJSON_AddNumberToObject(XY3, "Y", 9999);
        cJSON_AddItemToArray(PointList, XY3);

        cJSON *XY4 = cJSON_CreateObject();
        cJSON_AddNumberToObject(XY4, "X", 0);
        cJSON_AddNumberToObject(XY4, "Y", 9999);
        cJSON_AddItemToArray(PointList, XY4);

        cJSON_AddItemToObject(p1, "PointList", PointList);
    }

    cJSON *DetectTargetInfo_pd = NULL;
    smart_intrusion_target_info_get(pstItemAlarm, &DetectTargetInfo_pd, 2, area_index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_pd);

    cJSON *DetectTargetInfo_car = NULL;
    smart_intrusion_target_info_get(pstItemAlarm, &DetectTargetInfo_car, 0, area_index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_car);

    cJSON *DetectTargetInfo_noncar = NULL;
    smart_intrusion_target_info_get(pstItemAlarm, &DetectTargetInfo_noncar, 1, area_index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_noncar);

    cJSON_AddNumberToObject(p1, "Priority", 0);
    cJSON_AddNumberToObject(p1, "Num", 3);
    cJSON_AddItemToObject(p1, "DetectTargetList", DetectTargetList);

    *p = p1;
    return 0;
}

int unv_smart_intrusion_areas_get(cJSON **p, int area_index)
{
    cJSON *VehicleDetectAreaInfoList = cJSON_CreateObject(); 
    cJSON *PolygonInfoList           = cJSON_CreateArray(); 

    int Num = 4;
    int i = 0;
    for (i = 0; i < Num; i++)
    {
        cJSON *VehicleDetectPolygonInfo = NULL; 
        unv_smart_intrusion_one_area_get(&VehicleDetectPolygonInfo, area_index, i);
        cJSON_AddItemToArray(PolygonInfoList, VehicleDetectPolygonInfo);    
    }

    cJSON_AddNumberToObject(VehicleDetectAreaInfoList, "Num", Num);
    cJSON_AddItemToObject(VehicleDetectAreaInfoList, "PolygonInfoList", PolygonInfoList);

    *p = VehicleDetectAreaInfoList;
    return 0;
}

int unv_smart_intrusion_rule_get(cJSON **p, int flag)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[chn];
    
    cJSON *IntrusionDetectionRuleInfo = cJSON_CreateObject();
    if (pstItemAlarm->data[flag].enable == 1)
        cJSON_AddNumberToObject(IntrusionDetectionRuleInfo, "Enabled", 1);
    else
        cJSON_AddNumberToObject(IntrusionDetectionRuleInfo, "Enabled", 0);

    *p = IntrusionDetectionRuleInfo;
    return 0;
}

int unv_smart_intrusion_linkage_action_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[chn];

    if (1)
    {
        int Num = 0;
        cJSON *LinkageActionList = cJSON_CreateObject();
        cJSON *Actions = cJSON_CreateArray();

        /////////////////////////////////////////////////////音频
        if (1)
        {
            Num ++;
            cJSON *AudioActParamInfo = NULL;
            smart_intrusion_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 0);
            cJSON_AddItemToArray(Actions, AudioActParamInfo);
        }

        if (1)
        {
            Num ++;
            cJSON *AudioLinkagePlanInfo = NULL;
            smart_intrusion_link_audio_plan_info_get(pstItemAlarm, &AudioLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, AudioLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////

        /////////////////////////////////////////////////////灯光
        if (1)
        {
            Num ++;
            cJSON *LightActParamInfo = NULL;
            smart_intrusion_link_light_alarm_info_get(pstItemAlarm, &LightActParamInfo, 0);
            cJSON_AddItemToArray(Actions, LightActParamInfo);
        }

        if (0 || anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION) == 1)//延创兴版本/支持灯光闪烁能力集
        {
            Num ++;
            cJSON *LightLinkagePlanInfo = NULL;
            smart_intrusion_link_light_plan_info_get(pstItemAlarm, &LightLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
        }
        else                    //红蓝警灯
        {
            Num ++;
            cJSON *LightLinkagePlanInfo = NULL;
            smart_intrusion_link_brled_light_plan_info_get(pstItemAlarm, &LightLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////
        cJSON_AddNumberToObject(LinkageActionList, "Num", Num);
        cJSON_AddItemToObject(LinkageActionList, "Actions", Actions);

        *p = LinkageActionList;
    }

    return 0;
}

int unv_smart_intrusion_week_plan_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoRegionAiAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[chn];

    char str[33] = {0};
    char str1[24] = {0};
    str[32] = '\0';

    int Num = 0;
    int Week_Num = 0;
    int day = 0;
    int time = 0;

    cJSON *Week_Json = cJSON_CreateObject();
    cJSON *Days_arry = cJSON_CreateArray();
    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);

    for (day = 1; day <= 7; day++)
    {
        if (day == 7)
        {
            ToBin(pstItemAlarm->timeSpan.workday[0], str);
        }
        else
        {
            ToBin(pstItemAlarm->timeSpan.workday[day], str);
        }

        strcpy(str1, str + 8);
        if (strcmp(str1, "000000000000000000000000"))
        {
            cJSON *Day_Json=cJSON_CreateObject();
            cJSON *TimeSectionInfos_arry = cJSON_CreateArray();
            cJSON_AddNumberToObject(Day_Json, "ID", day);

            Num = 0;
            time = 0;

            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {    
                    char Begin[16] = {0};
                    char End[16]   = {0};
                    char Begin_type[12] = "%2d:00:00";
                    char End_type[12]   = "%2d:00:00";

                    cJSON *Time_Json = cJSON_CreateObject();
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");

                    Num++;
                    snprintf(Begin, sizeof(Begin), Begin_type, time);
                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);

                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            snprintf(End, sizeof(End),End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            snprintf(End, sizeof(End),End_type, time + 1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }                
                        else
                        {
                            time++;
                        }
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;    
            cJSON_AddNumberToObject(Day_Json, "Num", Num);
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);

    *p = Week_Json;
    return 0;
}


//-------------------------------------------------------------------------------------------
//                          smart autotrack
//-------------------------------------------------------------------------------------------

int smart_autotrack_capability_info_get(int track_enable, cJSON **p)
{
    int i = 0;
    cJSON *TrackingCapInfoJson = cJSON_CreateObject();

    if (track_enable == 1)
    {
        cJSON_AddNumberToObject(TrackingCapInfoJson, "IsSupport", 1);
        cJSON_AddNumberToObject(TrackingCapInfoJson, "SupportWeekPlan", 0);
        cJSON_AddNumberToObject(TrackingCapInfoJson, "SupportZoomOption", 0);

        // Mode字段根据宇视抓包需要上报数组[1, 0, 0, 0, 0, 0]
        cJSON *ModeArray = cJSON_CreateArray();
        cJSON_AddItemToArray(ModeArray, cJSON_CreateNumber(1));
        for (i = 0; i < 5; i++) 
        {
            cJSON_AddItemToArray(ModeArray, cJSON_CreateNumber(0));
        }
        cJSON_AddItemToObject(TrackingCapInfoJson, "Mode", ModeArray);

        cJSON *OverAllJson = cJSON_CreateObject();
        cJSON_AddNumberToObject(OverAllJson, "TrackTimeMin", 1);
        cJSON_AddNumberToObject(OverAllJson, "TrackTimeMax", 300);
        cJSON_AddItemToObject(TrackingCapInfoJson, "OverAll", OverAllJson);
    }
    else
    {
        cJSON_AddNumberToObject(TrackingCapInfoJson, "IsSupport", 0);
        cJSON_AddNumberToObject(TrackingCapInfoJson, "SupportWeekPlan", 0);
        cJSON_AddNumberToObject(TrackingCapInfoJson, "SupportZoomOption", 0);

        cJSON *ModeArray = cJSON_CreateArray();
        cJSON_AddItemToArray(ModeArray, cJSON_CreateNumber(1));
        for (i = 0; i < 5; i++) 
        {
            cJSON_AddItemToArray(ModeArray, cJSON_CreateNumber(0));
        }
        cJSON_AddItemToObject(TrackingCapInfoJson, "Mode", ModeArray);
    }

    *p = TrackingCapInfoJson;
    return 0;
}

int unv_smart_autotrack_rule_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    cJSON *TrackingRuleJson = cJSON_CreateObject();
    if (TrackingRuleJson == NULL)
    {
        return -1;
    }

    int track_enable = 0;
    if (pstItemAlarm->enable == 1 && pstItemAlarm->alarmAction.track_human_enable == 1)
    {
        track_enable = 1;
    }

    cJSON_AddNumberToObject(TrackingRuleJson, "Enabled", track_enable);
    cJSON_AddNumberToObject(TrackingRuleJson, "LastTrackEnabled", track_enable);
    cJSON_AddNumberToObject(TrackingRuleJson, "Mode", 1);

    cJSON *OverAllJson = cJSON_CreateObject();
    cJSON_AddNumberToObject(OverAllJson, "ZoomRatio", 0);
    cJSON_AddItemToObject(TrackingRuleJson, "OverAll", OverAllJson);

    cJSON *RuleJson = cJSON_CreateObject();
    cJSON_AddItemToObject(RuleJson, "Rule", TrackingRuleJson);

    *p = RuleJson;
    return 0;
}

#if 0
int Get_Tracking_LinkageActions(cJSON **p)
{
    printf("get tracking linkage actions\n");

    int Num = 0;
    cJSON *LinkageActionListJson = cJSON_CreateObject();
    cJSON *Actions = cJSON_CreateArray();

    /////////////////////////////////////////////////////音频
    if (1)
    {
        Num++;
        cJSON *AudioLinkagePlanInfo = NULL;
        Get_HumanTracking_LinkageActions_AudioLinkagePlanInfo(&AudioLinkagePlanInfo);
        cJSON_AddItemToArray(Actions, AudioLinkagePlanInfo);
    }
    /////////////////////////////////////////////////////

    /////////////////////////////////////////////////////灯光
    if (1)
    {
        Num++;
        cJSON *LightLinkagePlanInfo = NULL;
        Get_HumanTracking_LinkageActions_LightLinkagePlanInfo(&LightLinkagePlanInfo);
        cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
    }
    /////////////////////////////////////////////////////

    cJSON_AddNumberToObject(LinkageActionListJson, "Num", Num);
    cJSON_AddItemToObject(LinkageActionListJson, "Actions", Actions);

    *p = LinkageActionListJson;    //free 6
    return 0; 
}

int Get_Tracking_WeekPlan_Info(cJSON **p)
{
    printf("get tracking week plan info\n");
    //-----------------------------------------------------
    PdAlarm PdAlarmCfg = {0};
    MsgGetFdAlarm(&PdAlarmCfg);

    char *str = malloc(33); // free 1
    char *str1 = malloc(24); // free 2
    memset(str, 0, 33); 
    memset(str1, 0, 24);

    str[32] = '\0';
    cJSON *Week_Json;
    cJSON *Days_arry;
    int day, time;

    /*1.初始化week*/ 
    Week_Json = cJSON_CreateObject(); // free 3
    Days_arry = cJSON_CreateArray(); // free 4
    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);

    int Week_Num = 0;
    int Num = 0;
    //-----------------------------------------------------
    for (day = 1; day <= 7; day++)
    {
        //-------------------------------------------
        if (day == 7)
        {
            ToBin(PdAlarmCfg.timeSpan.workday[0], str);
        }
        else
            ToBin(PdAlarmCfg.timeSpan.workday[day], str);

        strcpy(str1, str+8);

        if (strcmp(str1, "000000000000000000000000"))
        {
            /*1.初始化DAY*/ 
            cJSON *Day_Json;
            cJSON *TimeSectionInfos_arry;
            Day_Json=cJSON_CreateObject(); // free 5
            TimeSectionInfos_arry = cJSON_CreateArray(); // free 6
            cJSON_AddNumberToObject(Day_Json, "ID", day);
            Num = 0;
            time = 0;
            //-------------------------------------------
            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {    
                    /*1.TimeSectionInfo*/
                    cJSON *Time_Json;
                    char Begin[16]     = {0};
                    char End[16]     = {0};
                    char Begin_type[16]     = "%2d:00:00";
                    char End_type[16]     = "%2d:00:00";
                    Time_Json = cJSON_CreateObject(); // free 7
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");
                    Num++;
                    sprintf(Begin, Begin_type, time);
                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);

                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            sprintf(End, End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            sprintf(End, End_type, time+1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else
                            time++;
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;    
            cJSON_AddNumberToObject(Day_Json, "Num", Num);
            /*3.结构体嵌套填充*/
            //向cJSON结构体province中添加cityArray数组对象
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    /*3.结构体嵌套填充*/
    //向cJSON结构体province中添加cityArray数组对象
    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);

    *p = Week_Json;

    if ( NULL != str )
    {
        free(str);
        str = NULL;
    }
    if ( NULL != str1 )
    {
        free(str1);
        str1 = NULL;
    }

    return 0;
    //-----------------------------------------------------
}
#endif


//-------------------------------------------------------------------------------------------
//                          smart face detect
//-------------------------------------------------------------------------------------------

int smart_facedetect_capability_info_get(int face_enable, cJSON **p)
{
    cJSON *FaceDetectionCapInfo = cJSON_CreateObject();

    if (face_enable)
    {
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "IsSupport", 1);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "QualityAnalysisIsSupport", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "AttributeAnalysisIsSupport", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "AttributeAnalysisSkills", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "PersonSnapshotSupport", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "FeatureIsSupport", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "RecognitionIsSupport", 0);
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "PupilDistanceSizeNum", 0);
        
        cJSON *PupilDistanceSizeList = cJSON_CreateArray();
        cJSON_AddItemToObject(FaceDetectionCapInfo, "PupilDistanceSizeList", PupilDistanceSizeList);
    }
    else
    {
        cJSON_AddNumberToObject(FaceDetectionCapInfo, "IsSupport", 0);
    }

    *p = FaceDetectionCapInfo;
    return 0;
}

int smart_facedetect_link_audio_alarm_info_get(FaceDetectAlarm *pstItemAlarm, cJSON **p, int flag)
{
    int i = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapCfg = &pstMediaCfg->audioConfig.audioCapture;

    cJSON *ActParam = cJSON_CreateObject();
    if (ActParam == NULL)
    {
        __ERR("cJSON_CreateObject ActParam failed\n");
        return -1;
    }

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);
    
    float vol_val = (float)pstAudioCapCfg->volume_play;
    vol_val = vol_val/100*255;
    cJSON_AddNumberToObject(ActParam, "AudioVolume", (int)vol_val);

    AudioFileList *pstAudioFileList = http_audio_file_list_get();
    for (i = 0; i < pstAudioFileList->Num; i++)
    {
        int len = strlen(pstItemAlarm->alarmAction.audioAction.filename);
        if(strncmp(pstAudioFileList->Item[i].file_pathname, pstItemAlarm->alarmAction.audioAction.filename, len) == 0)
            cJSON_AddNumberToObject(ActParam, "AudioFileID", pstAudioFileList->Item[i].ID);
    }

    if (pstItemAlarm->alarmAction.audioAction.times >= 0 && pstItemAlarm->alarmAction.audioAction.times < 9)
        cJSON_AddNumberToObject(ActParam, "WarnCount", pstItemAlarm->alarmAction.audioAction.times);
    else
        cJSON_AddNumberToObject(ActParam, "WarnCount", 9);

    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *AudioActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(AudioActParamInfo, "ActID", UNV_ACT_ID_AUDIO_ALARM);
        cJSON_AddItemToObject(AudioActParamInfo, "ActParam", ActParam);
        *p = AudioActParamInfo;
    }

    return 0;
}

int smart_facedetect_link_audio_day_plan_get(FaceDetectAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    int enable_flag = pstItemAlarm->alarmAction.audioAction.enable.enable_flag;

    cJSON *AudioCustomDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
            
            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.audioAction.enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));

                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.sec);
                
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
        else
        {
            for (i = 0; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
                
                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(AudioCustomDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = AudioCustomDayPlanInfo;
    return 0;
}

int smart_facedetect_link_audio_plan_info_get(FaceDetectAlarm *pstItemAlarm, cJSON **p)
{
    cJSON *AudioLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *AudioCustomInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(AudioLinkagePlanInfo, "ActID", UNV_ACT_ID_AUDIO_PLAN);

    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);

    cJSON_AddNumberToObject(AudioCustomInfo, "Num", 7);

    int i = 0;
    //获取周一的时间段copy7天
    for (i = 1; i <= 7; i++)
    {
        cJSON *AudioCustomDayPlanInfo = NULL;
        smart_facedetect_link_audio_day_plan_get(pstItemAlarm, &AudioCustomDayPlanInfo, i);
        cJSON_AddItemToArray(Days, AudioCustomDayPlanInfo);
    }

    cJSON_AddItemToObject(AudioCustomInfo, "Days", Days);
    cJSON_AddItemToObject(ActParam, "AudioCustomInfo", AudioCustomInfo);
    cJSON_AddItemToObject(AudioLinkagePlanInfo, "ActParam", ActParam);

    *p = AudioLinkagePlanInfo;
    return 0;
}

int unv_smart_facedetect_enable_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstFaceAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[chn];

    cJSON *FaceEnableInfo = cJSON_CreateObject();
    if (pstFaceAlarm->enable)
    {
        cJSON_AddNumberToObject(FaceEnableInfo, "Enabled", 1);
    }
    else
    {
        cJSON_AddNumberToObject(FaceEnableInfo, "Enabled", 0);
    }

    *p = FaceEnableInfo;
    return 0;
}

int unv_smart_facedetect_rule_get(cJSON **p)
{ 
    cJSON *FaceDetectionRuleInfo = cJSON_CreateObject();
    if (FaceDetectionRuleInfo == NULL)
    {
        __ERR("cJSON_CreateObject FaceDetectionRuleInfo failed\n");
        return -1;
    }

    cJSON *FaceCount = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceCount, "Enabled", 0);
    cJSON_AddNumberToObject(FaceCount, "Direction", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceCount", FaceCount);

    cJSON *FaceSnapshot = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceSnapshot, "Mode", 1);
    cJSON_AddNumberToObject(FaceSnapshot, "Num", 1);
    cJSON_AddNumberToObject(FaceSnapshot, "MarkFace", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceSnapshot", FaceSnapshot);

    cJSON *PersonSnapshot = cJSON_CreateObject();
    cJSON_AddNumberToObject(PersonSnapshot, "Enabled", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "PersonSnapshot", PersonSnapshot);

    cJSON *FaceQualityAnalysis = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceQualityAnalysis, "Enabled", 1);
    cJSON_AddNumberToObject(FaceQualityAnalysis, "Mode", 0);
    cJSON_AddNumberToObject(FaceQualityAnalysis, "PreferenceFastNum", 3);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceQualityAnalysis", FaceQualityAnalysis);

    cJSON *FaceQualityAnalysisEx = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "Enabled", 0);
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "Mode", 0);
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "QualityNum", 1);
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "PreferenceLastTime", 5);
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "IntervalTime", 1000);
    cJSON_AddNumberToObject(FaceQualityAnalysisEx, "IsUploadLarge", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceQualityAnalysisEx", FaceQualityAnalysisEx);

    cJSON *FaceAttributeAnalysis = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceAttributeAnalysis, "Enabled", 0);
    cJSON_AddNumberToObject(FaceAttributeAnalysis, "SexEnabled", 0);
    cJSON_AddNumberToObject(FaceAttributeAnalysis, "AgeEnabled", 0);
    cJSON_AddNumberToObject(FaceAttributeAnalysis, "GlassEnabled", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceAttributeAnalysis", FaceAttributeAnalysis);

    cJSON *FaceRecognition = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceRecognition, "Enabled", 0);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceRecognition", FaceRecognition);

    cJSON *FaceFullcheckInfo = cJSON_CreateObject();
    cJSON_AddNumberToObject(FaceFullcheckInfo, "Enabled", 1);
    cJSON_AddItemToObject(FaceDetectionRuleInfo, "FaceFullcheck", FaceFullcheckInfo);

    *p = FaceDetectionRuleInfo;
    return 0;
}

int unv_smart_facedetect_areas_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstFaceAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[chn];

    cJSON *FaceDetectionAreaList = cJSON_CreateObject();
    if (FaceDetectionAreaList == NULL)
    {
        __ERR("cJSON_CreateObject FaceDetectionAreaList failed\n");
        return -1;
    }

    if (pstFaceAlarm->area.xPos == 0 && pstFaceAlarm->area.yPos == 0 && 
        pstFaceAlarm->area.height == 100 && pstFaceAlarm->area.width == 100)
    {
        cJSON_AddNumberToObject(FaceDetectionAreaList, "EntireImageEnabled", 1);
    }
    else
    {
        cJSON_AddNumberToObject(FaceDetectionAreaList, "EntireImageEnabled", 0);
    }

    cJSON_AddNumberToObject(FaceDetectionAreaList, "Num", 1);
    if (1)
    {
        cJSON *Polygons                     = cJSON_CreateArray();
        cJSON *FaceDetectionPolygonInfo     = cJSON_CreateObject();
        cJSON_AddNumberToObject(FaceDetectionPolygonInfo, "ID", 0);

        if (pstFaceAlarm->enable)
            cJSON_AddNumberToObject(FaceDetectionPolygonInfo, "Enabled", 1);
        else
            cJSON_AddNumberToObject(FaceDetectionPolygonInfo, "Enabled", 0);

        cJSON_AddNumberToObject(FaceDetectionPolygonInfo, "Sensitivity", pstFaceAlarm->sensitivity);
        cJSON_AddNumberToObject(FaceDetectionPolygonInfo, "PointNum", 4);

        cJSON *Points = cJSON_CreateArray();
        cJSON *P1 = cJSON_CreateObject(); 
        cJSON *P2 = cJSON_CreateObject(); 
        cJSON *P3 = cJSON_CreateObject(); 
        cJSON *P4 = cJSON_CreateObject();

        cJSON_AddNumberToObject(P1, "X", (pstFaceAlarm->area.xPos)*100);
        cJSON_AddNumberToObject(P1, "Y", (pstFaceAlarm->area.yPos)*100);
        cJSON_AddNumberToObject(P2, "X", (pstFaceAlarm->area.xPos + pstFaceAlarm->area.width)*100);
        cJSON_AddNumberToObject(P2, "Y", (pstFaceAlarm->area.yPos)*100);
        cJSON_AddNumberToObject(P3, "X", (pstFaceAlarm->area.xPos + pstFaceAlarm->area.width)*100);
        cJSON_AddNumberToObject(P3, "Y", (pstFaceAlarm->area.yPos + pstFaceAlarm->area.height)*100);
        cJSON_AddNumberToObject(P4, "X", (pstFaceAlarm->area.xPos)*100);
        cJSON_AddNumberToObject(P4, "Y", (pstFaceAlarm->area.yPos + pstFaceAlarm->area.height)*100);

        cJSON_AddItemToArray(Points, P1); 
        cJSON_AddItemToArray(Points, P2); 
        cJSON_AddItemToArray(Points, P3); 
        cJSON_AddItemToArray(Points, P4);

        cJSON_AddItemToObject(FaceDetectionPolygonInfo, "Points", Points);
        cJSON_AddItemToObject(Polygons, "FaceDetectionPolygonInfo", FaceDetectionPolygonInfo);
        cJSON_AddItemToObject(FaceDetectionAreaList, "Polygons", Polygons);
    }

    *p = FaceDetectionAreaList;
    return 0;
}

int unv_smart_facedetect_linkage_action_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[chn];

    if (1)
    {
        int Num = 0;
        cJSON *LinkageActionList = cJSON_CreateObject();
        cJSON *Actions = cJSON_CreateArray();
        
        /////////////////////////////////////////////////////音频
        if (1)
        {
            Num ++;
            cJSON *AudioActParamInfo = NULL;
            smart_facedetect_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 0);
            cJSON_AddItemToArray(Actions, AudioActParamInfo);
        }
        if (1)
        {
            Num ++;
            cJSON *AudioLinkagePlanInfo = NULL;
            smart_facedetect_link_audio_plan_info_get(pstItemAlarm, &AudioLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, AudioLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////        
        cJSON_AddNumberToObject(LinkageActionList, "Num", Num);
        cJSON_AddItemToObject(LinkageActionList, "Actions", Actions);

        *p = LinkageActionList;
    }
    
    return 0;
}

int unv_smart_facedetect_week_plan_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    FaceDetectAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[chn];

    char str[33] = {0};
    char str1[24] = {0};
    str[32] = '\0';

    int Num = 0;
    int Week_Num = 0;
    int day = 0;
    int time = 0;

    cJSON *Week_Json = cJSON_CreateObject();
    cJSON *Days_arry = cJSON_CreateArray();
    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);

    for (day = 1; day <= 7; day++)
    {
        if (day == 7)
        {
            ToBin(pstItemAlarm->timeSpan.workday[0], str);
        }
        else
        {
            ToBin(pstItemAlarm->timeSpan.workday[day], str);
        }

        strcpy(str1, str+8);
        if (strcmp(str1, "000000000000000000000000"))
        {
            cJSON *Day_Json=cJSON_CreateObject();
            cJSON *TimeSectionInfos_arry = cJSON_CreateArray();
            cJSON_AddNumberToObject(Day_Json, "ID", day);

            Num = 0;
            time = 0;
            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {    
                    char Begin[16] = {0};
                    char End[16]   = {0};
                    char Begin_type[12] = "%2d:00:00";
                    char End_type[12]   = "%2d:00:00";

                    cJSON *Time_Json = cJSON_CreateObject();
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");

                    Num++;
                    snprintf(Begin, sizeof(Begin), Begin_type, time);
                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);

                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            snprintf(End, sizeof(End), End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            snprintf(End, sizeof(End), End_type, time + 1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else
                        {
                            time++;
                        }
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;    
            cJSON_AddNumberToObject(Day_Json, "Num", Num);
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);
    
    *p = Week_Json;
    return 0;
}

//-------------------------------------------------------------------------------------------
//                          smart crossline
//-------------------------------------------------------------------------------------------

int smart_crossline_capability_info_get(int vg_enable, int track_enable, cJSON **p)
{
    cJSON *CrossLineDetectionCapInfo = cJSON_CreateObject();
    cJSON *LinkagePlanCfg = cJSON_CreateObject();

    int unv_cross_line_mode = 1;
    if (track_enable == 1)             // 开启智能跟踪时，cross line mode要设置成0
    {
        unv_cross_line_mode = 0;
    }

    if (vg_enable)
    {

        cJSON_AddNumberToObject(CrossLineDetectionCapInfo, "SupportCfg", 1);
        cJSON_AddNumberToObject(CrossLineDetectionCapInfo, "Mode", unv_cross_line_mode);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 1);
    }
    else
    {
        cJSON_AddNumberToObject(CrossLineDetectionCapInfo, "SupportCfg", 0);
        cJSON_AddNumberToObject(CrossLineDetectionCapInfo, "Mode", unv_cross_line_mode);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 0);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 0);
    }

    cJSON_AddItemToObject(CrossLineDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    *p = CrossLineDetectionCapInfo;

    return 0;
}

int smart_crossline_target_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int type, int index)
{
    cJSON *DetectTargetInfo = cJSON_CreateObject();
    cJSON *ObjectMaxSize = cJSON_CreateObject();
    cJSON *ObjectMinSize = cJSON_CreateObject();
    
    int Enabled = 0;
    char type_buf[33] = {0};/*人车型是否使能*/
    type_buf[32] = '\0';

    ToBin(pstItemAlarm->data[index].type, type_buf);

    if (type == 2)//人2非1车0
    {
        if(type_buf[31-4] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;
            
        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 2);
    }
    else if(type == 1)//人2非1车0
    {
        if((type_buf[31-1] == 0x31) || (type_buf[31-2] == 0x31) || (type_buf[31-3] == 0x31) || (type_buf[31-6] == 0x31))
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 1);
    }
    else if(type == 0)//人2非1车0
    {
        if(type_buf[31-0] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 0);
    }

    cJSON_AddNumberToObject(ObjectMaxSize, "Width", 9999);
    cJSON_AddNumberToObject(ObjectMaxSize, "Height", 9999);
    cJSON_AddNumberToObject(ObjectMinSize, "Width", 0);
    cJSON_AddNumberToObject(ObjectMinSize, "Height", 0);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMaxSize", ObjectMaxSize);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMinSize", ObjectMinSize);

    *p = DetectTargetInfo;
    return 0;
}

int smart_crossline_link_audio_alarm_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int flag)
{
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCapCfg = &pstMediaCfg->audioConfig.audioCapture;

    cJSON *ActParam = cJSON_CreateObject();

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    float vol_val = (float)pstAudioCapCfg->volume_play;
    vol_val = vol_val / 100 * 255;
    cJSON_AddNumberToObject(ActParam, "AudioVolume", (int)vol_val);

    int i = 0;
    AudioFileList *pstAudioFileList = http_audio_file_list_get();
    for (i = 0; i < pstAudioFileList->Num; i++)
    {
        int len = strlen(pstItemAlarm->alarmAction.audioAction.filename);
        if(strncmp(pstAudioFileList->Item[i].file_pathname, pstItemAlarm->alarmAction.audioAction.filename, len) == 0)
            cJSON_AddNumberToObject(ActParam, "AudioFileID", pstAudioFileList->Item[i].ID);
    }

    if (pstItemAlarm->alarmAction.audioAction.times >= 0 && pstItemAlarm->alarmAction.audioAction.times < 9)
        cJSON_AddNumberToObject(ActParam, "WarnCount", pstItemAlarm->alarmAction.audioAction.times);
    else
        cJSON_AddNumberToObject(ActParam, "WarnCount", 9);

    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *AudioActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(AudioActParamInfo, "ActID", UNV_ACT_ID_AUDIO_ALARM);
        cJSON_AddItemToObject(AudioActParamInfo, "ActParam", ActParam);
        *p = AudioActParamInfo;
    }

    return 0;
}

int smart_crossline_link_audio_day_plan_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    int enable_flag = pstItemAlarm->alarmAction.audioAction.enable.enable_flag;

    cJSON *AudioCustomDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
            
            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.audioAction.enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));

                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.sec);
                
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
        else
        {
            for (i = 0; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 1);
                
                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(AudioCustomDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = AudioCustomDayPlanInfo;
    return 0;
}

int smart_crossline_link_audio_plan_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *AudioLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *AudioCustomInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(AudioLinkagePlanInfo, "ActID", UNV_ACT_ID_AUDIO_PLAN);
    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddNumberToObject(AudioCustomInfo, "Num", 7);

    //获取周一的时间段copy7天
    for (i = 1; i <= 7; i++)
    {
        cJSON *AudioCustomDayPlanInfo = NULL;
        smart_crossline_link_audio_day_plan_get(pstItemAlarm, &AudioCustomDayPlanInfo, i);
        cJSON_AddItemToArray(Days, AudioCustomDayPlanInfo);
    }
    
    cJSON_AddItemToObject(AudioCustomInfo, "Days", Days);
    cJSON_AddItemToObject(ActParam, "AudioCustomInfo", AudioCustomInfo);
    cJSON_AddItemToObject(AudioLinkagePlanInfo, "ActParam", ActParam);

    *p = AudioLinkagePlanInfo;
    return 0;
}

int smart_crossline_link_brled_light_alarm_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int flag)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *ActParam = cJSON_CreateObject();
    
    if (pstItemAlarm->alarmAction.alarm_led_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    cJSON_AddNumberToObject(ActParam, "Interval", 5);
    cJSON_AddNumberToObject(ActParam, "Luminance", pstVideoCapCfg->led_brightness_value * 10);
    
    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *LightActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(LightActParamInfo, "ActID", UNV_ACT_ID_LIGHT_ALARM);
        cJSON_AddItemToObject(LightActParamInfo, "ActParam", ActParam);
        *p = LightActParamInfo;
    }

    return 0;
}

int smart_crossline_link_light_alarm_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int flag)
{
    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    cJSON *ActParam = cJSON_CreateObject();

    if (pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);

    cJSON_AddNumberToObject(ActParam, "Interval", 5);
    cJSON_AddNumberToObject(ActParam, "Luminance", pstVideoCapCfg->led_brightness_value * 10);

    //char *LightActParamInfo_pri = NULL;
    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *LightActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(LightActParamInfo, "ActID", UNV_ACT_ID_LIGHT_ALARM);
        cJSON_AddItemToObject(LightActParamInfo, "ActParam", ActParam);
        *p = LightActParamInfo;
    }
    return 0;
}

int smart_crossline_link_brled_light_day_plan_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    //int Num_real = pstItemAlarm->alarmAction.alarm_led_enable.timespan_num;
    int enable_flag = pstItemAlarm->alarmAction.alarm_led_enable.enable_flag;

    cJSON *LightDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(LightDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(LightDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.alarm_led_enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.alarm_led_enable.timeSpans[i].endTime.sec);

                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
        else
        {
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(LightDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = LightDayPlanInfo;
    return 0;
}

int smart_crossline_link_light_day_plan_get(VideoGateAlarm *pstItemAlarm, cJSON **p, int ID)
{
    int i = 0;
    //int Num_real = pstItemAlarm->alarmAction.light_twinkle_enable.timespan_num;
    int enable_flag = pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag;

    cJSON *LightDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(LightDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(LightDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        for (i = 0; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        for (i = 1; i < 4; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstItemAlarm->alarmAction.light_twinkle_enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);

                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.hour,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.minute,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.hour,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.minute,
                    pstItemAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.sec);

                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
        else
        {
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(LightDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = LightDayPlanInfo;
    return 0;
}

int smart_crossline_link_brled_light_plan_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *LightLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *LightCustomInfo = cJSON_CreateObject();
    cJSON *LightWeekPlanInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.alarm_led_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(LightLinkagePlanInfo, "ActID", UNV_ACT_ID_LIGHT_PLAN);
    cJSON_AddNumberToObject(LightWeekPlanInfo, "Num", 7);
    
    //获取周一的时间段copy7天
    for (i = 1; i <= 7; i++)
    {
        cJSON *LightDayPlanInfo = NULL;
        smart_crossline_link_brled_light_day_plan_get(pstItemAlarm, &LightDayPlanInfo, i);
        cJSON_AddItemToArray(Days, LightDayPlanInfo);
    }
    cJSON_AddItemToObject(LightWeekPlanInfo, "Days", Days);

    cJSON *LightActParamInfo = NULL;
    smart_crossline_link_brled_light_alarm_info_get(pstItemAlarm, &LightActParamInfo, 1);

    cJSON_AddItemToObject(LightCustomInfo, "LightActParamInfo", LightActParamInfo);
    cJSON_AddItemToObject(LightCustomInfo, "LightWeekPlanInfo", LightWeekPlanInfo);
    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddItemToObject(ActParam, "LightCustomInfo", LightCustomInfo);

    cJSON_AddItemToObject(LightLinkagePlanInfo, "ActParam", ActParam);

    *p = LightLinkagePlanInfo;
    return 0;
}

int smart_crossline_link_light_plan_info_get(VideoGateAlarm *pstItemAlarm, cJSON **p)
{
    int i = 0;

    cJSON *LightLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *LightCustomInfo = cJSON_CreateObject();
    cJSON *LightWeekPlanInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();

    if (pstItemAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 1);

    cJSON_AddNumberToObject(LightLinkagePlanInfo, "ActID", UNV_ACT_ID_LIGHT_PLAN);
    cJSON_AddNumberToObject(LightWeekPlanInfo, "Num", 7);
    
    //获取周一的时间段copy7天
    for (i = 1; i <= 7; i++)
    {
        cJSON *LightDayPlanInfo = NULL;
        smart_crossline_link_light_day_plan_get(pstItemAlarm, &LightDayPlanInfo, i);
        cJSON_AddItemToArray(Days, LightDayPlanInfo);
    }
    
    cJSON_AddItemToObject(LightWeekPlanInfo, "Days", Days);

    cJSON *LightActParamInfo = NULL;
    smart_crossline_link_light_alarm_info_get(pstItemAlarm, &LightActParamInfo, 1);

    cJSON_AddItemToObject(LightCustomInfo, "LightActParamInfo", LightActParamInfo);
    cJSON_AddItemToObject(LightCustomInfo, "LightWeekPlanInfo", LightWeekPlanInfo);

    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddItemToObject(ActParam, "LightCustomInfo", LightCustomInfo);
  
    cJSON_AddItemToObject(LightLinkagePlanInfo, "ActParam", ActParam);

    *p = LightLinkagePlanInfo;
    return 0;
}

int unv_smart_crossline_one_area_get(cJSON **p, int index)
{ 
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[chn];
    
    cJSON *CrossLineDetectionLineInfo = cJSON_CreateObject();
    cJSON *StartPoint = cJSON_CreateObject();
    cJSON *EndPoint = cJSON_CreateObject();
    
    cJSON *DetectTargetList = cJSON_CreateArray();
    cJSON_AddNumberToObject(StartPoint, "X", pstItemAlarm->data[index].x0Pos * 100);
    cJSON_AddNumberToObject(StartPoint, "Y", pstItemAlarm->data[index].y0Pos * 100);
    cJSON_AddNumberToObject(EndPoint, "X", pstItemAlarm->data[index].x1Pos * 100);
    cJSON_AddNumberToObject(EndPoint, "Y", pstItemAlarm->data[index].y1Pos * 100);

    cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "ID", index);
    cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Enabled", pstItemAlarm->data[index].enable);
    cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Sensitivity", pstItemAlarm->data[index].sensitivity);

    if (pstItemAlarm->data[index].direction == 0)
        cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Direction", 0);
    else if(pstItemAlarm->data[index].direction == 1)
        cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Direction", 2);
    else if(pstItemAlarm->data[index].direction == 2)
        cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Direction", 1);
    else
        cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Direction", 0);

    cJSON_AddItemToObject(CrossLineDetectionLineInfo, "StartPoint", StartPoint);
    cJSON_AddItemToObject(CrossLineDetectionLineInfo, "EndPoint", EndPoint);

    cJSON *DetectTargetInfo_pd = NULL;
    smart_crossline_target_info_get(pstItemAlarm, &DetectTargetInfo_pd, 2, index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_pd);

    cJSON *DetectTargetInfo_car = NULL;
    smart_crossline_target_info_get(pstItemAlarm, &DetectTargetInfo_car, 0, index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_car);

    cJSON *DetectTargetInfo_noncar = NULL;
    smart_crossline_target_info_get(pstItemAlarm, &DetectTargetInfo_noncar, 1, index);
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_noncar);

    cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Priority", 0);
    cJSON_AddNumberToObject(CrossLineDetectionLineInfo, "Num", 3);
    cJSON_AddItemToObject(CrossLineDetectionLineInfo, "DetectTargetList", DetectTargetList);

    *p = CrossLineDetectionLineInfo;
    return 0;
}

int unv_smart_crossline_areas_get(cJSON **p)
{
    cJSON *CrossLineDetectionAreaInfoList = cJSON_CreateObject();
    cJSON *LineInfoList = cJSON_CreateArray();
    cJSON_AddNumberToObject(CrossLineDetectionAreaInfoList, "Num", 4);

    int i = 0;
    for (i = 0; i < 4; i++)
    {
        cJSON *CrossLineDetectionLineInfo = NULL;
        unv_smart_crossline_one_area_get(&CrossLineDetectionLineInfo, i);
        cJSON_AddItemToArray(LineInfoList, CrossLineDetectionLineInfo);
    }
    cJSON_AddItemToObject(CrossLineDetectionAreaInfoList, "LineInfoList", LineInfoList);

    *p = CrossLineDetectionAreaInfoList;
    return 0;
}

int unv_smart_crossline_rule_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[chn];

    cJSON *CrossLineDetectionRuleInfo = cJSON_CreateObject();
    if (pstItemAlarm->enable == 0)
        cJSON_AddNumberToObject(CrossLineDetectionRuleInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(CrossLineDetectionRuleInfo, "Enabled", 1);

    *p = CrossLineDetectionRuleInfo;
    return 0;
}

int unv_smart_crossline_linkage_action_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[chn];

    if (1)
    {
        int Num = 0;
        cJSON *LinkageActionList = cJSON_CreateObject();
        cJSON *Actions = cJSON_CreateArray();

        /////////////////////////////////////////////////////音频
        if (1)
        {
            Num ++;
            cJSON *AudioActParamInfo = NULL;
            smart_crossline_link_audio_alarm_info_get(pstItemAlarm, &AudioActParamInfo, 0);
            cJSON_AddItemToArray(Actions, AudioActParamInfo);
        }

        if (1)
        {
            Num ++;
            cJSON *AudioLinkagePlanInfo = NULL;
            smart_crossline_link_audio_plan_info_get(pstItemAlarm, &AudioLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, AudioLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////

        /////////////////////////////////////////////////////灯光
        if (1)
        {
            Num ++;
            cJSON *LightActParamInfo = NULL;
            smart_crossline_link_light_alarm_info_get(pstItemAlarm, &LightActParamInfo, 0);
            cJSON_AddItemToArray(Actions, LightActParamInfo);
        }

        if (0 || anj_sysctl_capability_check(FUNCTION_LIGHT_ACTION) == 1)   //延创兴版本/支持灯光闪烁能力集
        {
            Num ++;
            cJSON *LightLinkagePlanInfo = NULL;
            smart_crossline_link_light_plan_info_get(pstItemAlarm, &LightLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
        }
        else                    //红蓝警灯
        {
            Num ++;
            cJSON *LightLinkagePlanInfo = NULL;
            smart_crossline_link_brled_light_plan_info_get(pstItemAlarm, &LightLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////

        cJSON_AddNumberToObject(LinkageActionList, "Num", Num);
        cJSON_AddItemToObject(LinkageActionList, "Actions", Actions);

        *p = LinkageActionList;
    }

    return 0;
}

int unv_smart_crossline_week_plan_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    VideoGateAlarm *pstItemAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[chn];
    
    char str[33] = {0};
    char str1[24] = {0};
    str[32] = '\0';

    int Num = 0;
    int Week_Num = 0;
    int day = 0;
    int time = 0;

    cJSON *Week_Json = cJSON_CreateObject();
    cJSON *Days_arry = cJSON_CreateArray();
    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);

    for (day = 1; day <= 7; day++)
    {
        if (day == 7)
        {
            ToBin(pstItemAlarm->timeSpan.workday[0], str);
        }
        else
        {
            ToBin(pstItemAlarm->timeSpan.workday[day], str);
        }

        strcpy(str1, str + 8);
        if (strcmp(str1, "000000000000000000000000"))
        { 
            cJSON *Day_Json=cJSON_CreateObject();
            cJSON *TimeSectionInfos_arry = cJSON_CreateArray();
            cJSON_AddNumberToObject(Day_Json, "ID", day);

            Num = 0;
            time = 0;

            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {
                    char Begin[16] = {0};
                    char End[16]   = {0};
                    char Begin_type[12] = "%2d:00:00";
                    char End_type[12]   = "%2d:00:00";
                    cJSON *Time_Json = cJSON_CreateObject();
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");

                    Num++;
                    snprintf(Begin, sizeof(Begin), Begin_type, time);

                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);
                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            snprintf(End, sizeof(End), End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            snprintf(End, sizeof(End), End_type, time + 1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else
                        {
                            time++;
                        }
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;    
            cJSON_AddNumberToObject(Day_Json, "Num", Num);
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);

    *p = Week_Json;
    return 0;
}


//-------------------------------------------------------------------------------------------
//                          alarm smart_motion
//-------------------------------------------------------------------------------------------

int alarm_smart_motion_capability_info_ex_get(int smart_enable, cJSON **p)//智能人型
{
    int anNum[3] = {0};     // 0-车型 1-非机动车 2-人形
    cJSON *SmartMotionDetectionCapInfo     = cJSON_CreateObject();
    cJSON *LinkagePlanCfg                 = cJSON_CreateObject();
    cJSON *SupportDetectObjectList         = NULL;

    if(smart_enable)
    {
        cJSON_AddNumberToObject(SmartMotionDetectionCapInfo, "SupportCfg", 1);

        if (anj_sysctl_capability_check(FUNCTION_ALARM_PD))
        {
            anNum[2] = 1;
        }

        if (anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_CAR) ||
            anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_MOTO) ||
            anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_ELECTRICBICYCLE) ||
            anj_sysctl_capability_check(FUNCTION_ALARM_VEHICLE_BICYCLE))
        {
            anNum[0] = 1;
            anNum[1] = 1;
        }

        SupportDetectObjectList = cJSON_CreateIntArray(anNum, 3);
        cJSON_AddItemToObject(SmartMotionDetectionCapInfo, "SupportDetectObjectList", SupportDetectObjectList);

        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 1);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 1);

        cJSON_AddItemToObject(SmartMotionDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    }
    else
    {
        cJSON_AddNumberToObject(SmartMotionDetectionCapInfo, "SupportCfg", 0);

        SupportDetectObjectList            = cJSON_CreateIntArray(anNum, 3);
        cJSON_AddItemToObject(SmartMotionDetectionCapInfo, "SupportDetectObjectList", SupportDetectObjectList);

        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportAudioLinkagePlan", 0);
        cJSON_AddNumberToObject(LinkagePlanCfg, "SupportLightLinkagePlan", 0);

        cJSON_AddItemToObject(SmartMotionDetectionCapInfo, "LinkagePlanCfg", LinkagePlanCfg);
    }

    *p = SmartMotionDetectionCapInfo;
    return 0;
}

int alarm_smart_motion_target_info_get(PdAlarm *pstPdAlarm, cJSON **p, int flag)
{
    cJSON *DetectTargetInfo = cJSON_CreateObject();
    cJSON *ObjectMaxSize = cJSON_CreateObject();
    cJSON *ObjectMinSize = cJSON_CreateObject();

    int Enabled = 0;
    char type_buf[33] = {0};
    type_buf[32] = '\0';
    ToBin(pstPdAlarm->type, type_buf);

    if (flag == 2)      // 人形
    {
        if(type_buf[31 - 4] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 2);
    }
    else if(flag == 1)  // 非机动车
    {
        if((type_buf[31 - 1] == 0x31) || (type_buf[31 - 2] == 0x31) || (type_buf[31 - 3] == 0x31) || (type_buf[31 - 6] == 0x31))
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 1);
    }
    else if(flag == 0)  // 车型
    {
        if(type_buf[31 - 0] == 0x31)
            Enabled = 1;
        else
            Enabled = 0;

        cJSON_AddNumberToObject(DetectTargetInfo, "Enabled", Enabled);
        cJSON_AddNumberToObject(DetectTargetInfo, "Type", 0);
    }

    cJSON_AddNumberToObject(ObjectMaxSize, "Width", 9999);
    cJSON_AddNumberToObject(ObjectMaxSize, "Height", 9999);
    cJSON_AddNumberToObject(ObjectMinSize, "Width", 0);
    cJSON_AddNumberToObject(ObjectMinSize, "Height", 0);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMaxSize", ObjectMaxSize);
    cJSON_AddItemToObject(DetectTargetInfo, "ObjectMinSize", ObjectMinSize);

    *p = DetectTargetInfo;
    return 0;
}

int alarm_motion_link_audio_alarm_info_get(PdAlarm *pstPdAlarm, cJSON **p, int flag)
{
    int i = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioCapture *pstAudioCaptureCfg = &pstMediaCfg->audioConfig.audioCapture;

    cJSON *ActParam = cJSON_CreateObject();
    if (ActParam == NULL)
    {
        __ERR("cJSON_CreateObject ActParam failed\n");
        return -1;
    }

    if (pstPdAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);
    
    float vol_val = (float)pstAudioCaptureCfg->volume_play;
    vol_val = vol_val / 100 * 255;
    cJSON_AddNumberToObject(ActParam, "AudioVolume", (int)vol_val);

    AudioFileList *pstAudioFileList = http_audio_file_list_get();
    for (i = 0; i < pstAudioFileList->Num; i++)
    {
        int len = strlen(pstPdAlarm->alarmAction.audioAction.filename);
        if(strncmp(pstAudioFileList->Item[i].file_pathname, pstPdAlarm->alarmAction.audioAction.filename, len) == 0)
            cJSON_AddNumberToObject(ActParam, "AudioFileID", pstAudioFileList->Item[i].ID);
    }
    
    if (pstPdAlarm->alarmAction.audioAction.times >= 0 && pstPdAlarm->alarmAction.audioAction.times < 9)
        cJSON_AddNumberToObject(ActParam, "WarnCount", pstPdAlarm->alarmAction.audioAction.times);
    else
        cJSON_AddNumberToObject(ActParam, "WarnCount", 9);
    
    if (flag == 1)
        *p = ActParam;
    else
    {
        cJSON *AudioActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(AudioActParamInfo, "ActID", UNV_ACT_ID_AUDIO_ALARM);
        cJSON_AddItemToObject(AudioActParamInfo, "ActParam", ActParam);
        *p = AudioActParamInfo;
    }

    return 0;
}

int alarm_motion_link_audio_day_plan_get(PdAlarm *pstPdAlarm, cJSON **p, int ID)
{
    int i = 0;
    int enable_flag = pstPdAlarm->alarmAction.audioAction.enable.enable_flag;

    cJSON *AudioCustomDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    cJSON_AddNumberToObject(AudioCustomDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        if (1)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);

            for (i = 1; i < 4; i++)
            {
                cJSON *AudioTimeSectionInfo_t = cJSON_Duplicate(AudioTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        if (1)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);

            for (i = 2; i < 4; i++)
            {
                cJSON *AudioTimeSectionInfo_t = cJSON_Duplicate(AudioTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        if (1)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();

            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);
            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);

            for (i = 2; i < 4; i++)
            {
                cJSON *AudioTimeSectionInfo_t = cJSON_Duplicate(AudioTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
        }
        if (1)
        {
            cJSON *AudioActParamInfo = NULL;
            cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

            cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
            cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);

            for (i = 2; i < 4; i++)
            {
                cJSON *AudioTimeSectionInfo_t = cJSON_Duplicate(AudioTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        int timespan_num = pstPdAlarm->alarmAction.audioAction.enable.timespan_num;
        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                memset(time_buf_Begin, 0, 16);
                memset(time_buf_End, 0, 16);

                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);

                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.hour,
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.minute,
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.hour,
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.minute,
                    pstPdAlarm->alarmAction.audioAction.enable.timeSpans[i].endTime.sec);
                
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }

            for (i = timespan_num; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
        else
        {
            for (i = 0; i < 4; i++)
            {
                cJSON *AudioActParamInfo = NULL;
                cJSON *AudioTimeSectionInfo = cJSON_CreateObject();
                alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 1);

                cJSON_AddNumberToObject(AudioTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(AudioTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(AudioTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToObject(AudioTimeSectionInfo, "AudioActParamInfo", AudioActParamInfo);
                cJSON_AddItemToArray(TimeSectionInfos, AudioTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(AudioCustomDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = AudioCustomDayPlanInfo;
    return 0;
}

int alarm_motion_link_audio_plan_info_get(PdAlarm *pstPdAlarm, cJSON **p)
{
    int i = 0;

    cJSON *AudioLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *AudioCustomInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();
    
    if (pstPdAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(AudioLinkagePlanInfo, "Enabled", 1);
    
    cJSON_AddNumberToObject(AudioLinkagePlanInfo, "ActID", UNV_ACT_ID_AUDIO_PLAN);
    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddNumberToObject(AudioCustomInfo, "Num", 7);

    cJSON *AudioCustomDayPlanInfo = NULL;
    alarm_motion_link_audio_day_plan_get(pstPdAlarm, &AudioCustomDayPlanInfo, i);

    if (AudioCustomDayPlanInfo != NULL)
    {
        //获取周一的时间段copy7天
        for (i = 1; i <= 7; i++)
        {
            cJSON *AudioCustomDayPlanInfo_t = cJSON_Duplicate(AudioCustomDayPlanInfo, 1);
            cJSON_AddNumberToObject(AudioCustomDayPlanInfo_t, "ID", i);
            cJSON_AddItemToArray(Days, AudioCustomDayPlanInfo_t);
        }
        cJSON_Delete(AudioCustomDayPlanInfo);
    }

    cJSON_AddItemToObject(AudioCustomInfo, "Days", Days);
    cJSON_AddItemToObject(ActParam, "AudioCustomInfo", AudioCustomInfo);
    cJSON_AddItemToObject(AudioLinkagePlanInfo, "ActParam", ActParam);

    *p = AudioLinkagePlanInfo;
    return 0;
}

int alarm_motion_link_light_alarm_info_get(PdAlarm *pstPdAlarm, cJSON **p, int flag)
{
    cJSON *ActParam = cJSON_CreateObject();
    if (ActParam == NULL)
    {
        __ERR("cJSON_CreateObject ActParam failed\n");
        return -1;
    }

    int chn = 0;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    VideoCaptureCfg *pstVideoCapCfg = &pstMediaCfg->videoConfig[chn].videoCapture;

    if (pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(ActParam, "Enabled", 0);
    else
        cJSON_AddNumberToObject(ActParam, "Enabled", 1);
    
    cJSON_AddNumberToObject(ActParam, "Interval", 5);
    cJSON_AddNumberToObject(ActParam, "Luminance", pstVideoCapCfg->led_brightness_value * 10);
    
    if (flag == 1)
    {
        *p = ActParam;
    }
    else
    {
        cJSON *LightActParamInfo = cJSON_CreateObject();
        cJSON_AddNumberToObject(LightActParamInfo, "ActID", UNV_ACT_ID_LIGHT_ALARM);
        cJSON_AddItemToObject(LightActParamInfo, "ActParam", ActParam);
        *p = LightActParamInfo;
    }

    return 0;
}

int alarm_motion_link_light_day_plan_get(PdAlarm *pstPdAlarm, cJSON **p, int ID)
{
    int i = 0;
    //int Num_real = pstPdAlarm->alarmAction.light_twinkle_enable.timespan_num;
    int enable_flag = pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag;

    cJSON *LightDayPlanInfo = cJSON_CreateObject();
    cJSON *TimeSectionInfos = cJSON_CreateArray();
    //cJSON_AddNumberToObject(LightDayPlanInfo, "ID", ID);
    cJSON_AddNumberToObject(LightDayPlanInfo, "Num", 4);

    if (enable_flag == 0)//禁用
    {
        if (1)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            for (i = 1; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo_t = cJSON_Duplicate(LightTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 1)//全天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        if (1)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            for (i = 2; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo_t = cJSON_Duplicate(LightTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 2)//白天布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "18:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        if (1)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            for (i = 2; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo_t = cJSON_Duplicate(LightTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 3)//夜晚布防
    {
        for (i = 0; i < 1; i++)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "19:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "23:59:59");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
        }
        if (1)
        {
            cJSON *LightTimeSectionInfo = cJSON_CreateObject();
            cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
            cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
            cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
            cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            for (i = 2; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo_t = cJSON_Duplicate(LightTimeSectionInfo, 1);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo_t);
            }
        }
    }
    else if(enable_flag == 4)//自定义布防
    {
        int timespan_num = pstPdAlarm->alarmAction.light_twinkle_enable.timespan_num;
        char time_buf_Begin[16] = {0};
        char time_buf_End[16] = {0};

        if (timespan_num > 0)
        {
            for (i = 0; i < timespan_num; i++)
            {
                memset(time_buf_Begin, 0, sizeof(time_buf_Begin));
                memset(time_buf_End, 0, sizeof(time_buf_End));

                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                
                snprintf(time_buf_Begin, sizeof(time_buf_Begin), "%02d:%02d:%02d", 
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.hour,
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.minute,
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].startTime.sec);

                snprintf(time_buf_End, sizeof(time_buf_End), "%02d:%02d:%02d", 
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.hour,
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.minute,
                    pstPdAlarm->alarmAction.light_twinkle_enable.timeSpans[i].endTime.sec);

                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", time_buf_Begin);
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", time_buf_End);
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
        else
        {
            for (i = timespan_num; i < 4; i++)
            {
                cJSON *LightTimeSectionInfo = cJSON_CreateObject();
                cJSON_AddNumberToObject(LightTimeSectionInfo, "Enabled", 1);
                cJSON_AddStringToObject(LightTimeSectionInfo, "Begin", "00:00:00");
                cJSON_AddStringToObject(LightTimeSectionInfo, "End", "00:00:00");
                cJSON_AddItemToArray(TimeSectionInfos, LightTimeSectionInfo);
            }
        }
    }

    cJSON_AddItemToObject(LightDayPlanInfo, "TimeSectionInfos", TimeSectionInfos);

    *p = LightDayPlanInfo;
    return 0;
}

int alarm_motion_link_light_plan_info_get(PdAlarm *pstPdAlarm, cJSON **p)
{
    int i = 0;

    cJSON *LightLinkagePlanInfo = cJSON_CreateObject();
    cJSON *ActParam = cJSON_CreateObject();
    cJSON *LightCustomInfo = cJSON_CreateObject();
    cJSON *LightWeekPlanInfo = cJSON_CreateObject();
    cJSON *Days = cJSON_CreateArray();
    
    if (pstPdAlarm->alarmAction.light_twinkle_enable.enable_flag == ARMING_DISABLE)
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 0);
    else
        cJSON_AddNumberToObject(LightLinkagePlanInfo, "Enabled", 1);
    
    cJSON_AddNumberToObject(LightLinkagePlanInfo, "ActID", 29);

    cJSON_AddNumberToObject(LightWeekPlanInfo, "Num", 7);

    cJSON *LightDayPlanInfo = NULL;
    alarm_motion_link_light_day_plan_get(pstPdAlarm, &LightDayPlanInfo, i);
    if (LightDayPlanInfo != NULL)
    {
        //获取周一的时间段copy7天
        for (i = 1; i <= 7; i++)
        {
            cJSON *LightDayPlanInfo_t = cJSON_Duplicate(LightDayPlanInfo, 1);
            cJSON_AddNumberToObject(LightDayPlanInfo_t, "ID", i);
            cJSON_AddItemToArray(Days, LightDayPlanInfo_t);
        }
        cJSON_Delete(LightDayPlanInfo);
    }

    cJSON_AddItemToObject(LightWeekPlanInfo, "Days", Days);

    cJSON *LightActParamInfo = NULL;
    alarm_motion_link_light_alarm_info_get(pstPdAlarm, &LightActParamInfo, 1);
    
    cJSON_AddItemToObject(LightCustomInfo, "LightActParamInfo", LightActParamInfo);
    cJSON_AddItemToObject(LightCustomInfo, "LightWeekPlanInfo", LightWeekPlanInfo);

    cJSON_AddNumberToObject(ActParam, "TimeMode", 0);
    cJSON_AddItemToObject(ActParam, "LightCustomInfo", LightCustomInfo);
    cJSON_AddItemToObject(LightLinkagePlanInfo, "ActParam", ActParam);

    *p = LightLinkagePlanInfo;
    return 0;
}

int unv_alarm_smart_motion_week_plan_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    MotionDetectAlarm *pstMotionAlarm = &pstAlarmCfg->normalAlarm.motionDetectAlarm[chn];

    char str[33] = {0};
    char str1[24] = {0};
    str[32] = '\0';

    int Num = 0;
    int Week_Num = 0;
    int day = 0;
    int time = 0;
 
    cJSON *Week_Json = cJSON_CreateObject();
    cJSON *Days_arry = cJSON_CreateArray();
    cJSON_AddNumberToObject(Week_Json, "Enabled", 1);

    for (day = 1; day <= 7; day++)
    {
        if (day == 7)
        {
            ToBin(pstMotionAlarm->timeSpan.workday[0], str);
        }
        else
        {
            ToBin(pstMotionAlarm->timeSpan.workday[day], str);
        }

        strcpy(str1, str+8);
        if (strcmp(str1, "000000000000000000000000"))
        {
            cJSON *Day_Json=cJSON_CreateObject();
            cJSON *TimeSectionInfos_arry = cJSON_CreateArray();
            cJSON_AddNumberToObject(Day_Json, "ID", day);
            Num = 0;
            time = 0;

            while(time < 24)
            {
                if (str1[23 - time] == 49)
                {
                    char Begin[16] = {0};
                    char End[16]   = {0};
                    char Begint_type[12]  = "%2d:00:00";
                    char End_type[12]     = "%2d:00:00";

                    cJSON *Time_Json=cJSON_CreateObject();
                    cJSON_AddStringToObject(Time_Json, "ArmingType", "0");

                    Num++;
                    snprintf(Begin, sizeof(Begin), Begint_type, time);
                    cJSON_AddStringToObject(Time_Json, "Begin", Begin);

                    while (time < 24)
                    {
                        if (str1[23 - time] == 48)
                        {
                            snprintf(End, sizeof(End), End_type, time);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }
                        else if(time == 23)
                        {
                            snprintf(End, sizeof(End), End_type, time + 1);
                            cJSON_AddStringToObject(Time_Json, "End", End);
                            break;
                        }                
                        else
                        {
                            time++;
                        }
                    }

                    time++;
                    cJSON_AddItemToArray(TimeSectionInfos_arry, Time_Json);
                }
                else 
                {
                    time++;
                }
            }

            Week_Num++;
            cJSON_AddNumberToObject(Day_Json, "Num", Num);
            cJSON_AddItemToObject(Day_Json, "TimeSectionInfos", TimeSectionInfos_arry);
            cJSON_AddItemToArray(Days_arry, Day_Json);
        }
    }

    cJSON_AddNumberToObject(Week_Json, "Num", Week_Num);
    cJSON_AddItemToObject(Week_Json, "Days", Days_arry);
    *p = Week_Json;

    return 0;
}


int unv_alarm_smart_motion_rule_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];
    MotionDetectAlarm *pstMotionAlarm = &pstAlarmCfg->normalAlarm.motionDetectAlarm[chn];

    cJSON *RuleInfo = cJSON_CreateObject();
    if (RuleInfo == NULL)
    {
        __ERR("cJSON_CreateObject RuleInfo failed\n");
        return -1;
    }

    int motion_mode = unv_motion_detect_mode_get();
    if (motion_mode == 1)
    {
        if (pstPdAlarm->enable)
            cJSON_AddNumberToObject(RuleInfo, "Enabled", 1);
        else
            cJSON_AddNumberToObject(RuleInfo, "Enabled", 0);
        cJSON_AddNumberToObject(RuleInfo, "Mode", 1);
    }
    else if (motion_mode == 0)
    {
        if (pstMotionAlarm->enable)
            cJSON_AddNumberToObject(RuleInfo, "Enabled", 1);
        else
            cJSON_AddNumberToObject(RuleInfo, "Enabled", 0);
        cJSON_AddNumberToObject(RuleInfo, "Mode", 0);
    }
    else
    {
        if (pstPdAlarm->enable == 1)
        {
            cJSON_AddNumberToObject(RuleInfo, "Enabled", 1);
            cJSON_AddNumberToObject(RuleInfo, "Mode", 1);
        }
        else
        {
            if (pstMotionAlarm->enable == 1)
                cJSON_AddNumberToObject(RuleInfo, "Enabled", 1);
            else
                cJSON_AddNumberToObject(RuleInfo, "Enabled", 0);
            
            cJSON_AddNumberToObject(RuleInfo, "Mode", 0);
        }
    }

    *p = RuleInfo;
    return 0;
}

int unv_alarm_smart_motion_areas_get(cJSON **p)
{
    cJSON *VehicleDetectAreaInfoList  = cJSON_CreateObject();
    cJSON *PolygonInfoList            = cJSON_CreateArray();

    int Num = 1;
    int i = 0;
    cJSON_AddNumberToObject(VehicleDetectAreaInfoList, "Num", Num);
    for (i = 0; i < Num; i++)
    {
        cJSON *VehicleDetectPolygonInfo = NULL;
        unv_alarm_smart_motion_areas0_get(&VehicleDetectPolygonInfo);
        cJSON_AddItemToArray(PolygonInfoList, VehicleDetectPolygonInfo);    
    }
    cJSON_AddItemToObject(VehicleDetectAreaInfoList, "RECTAreas", PolygonInfoList);

    *p = VehicleDetectAreaInfoList;
    return 0;
}

int unv_alarm_smart_motion_areas0_get(cJSON **p)
{

    cJSON *p1           = cJSON_CreateObject();
    cJSON *AreaInfoList = cJSON_CreateObject();
    cJSON *PointList    = cJSON_CreateArray();
    
    cJSON *RECTArea     = cJSON_CreateObject();
    cJSON *TopLeft      = cJSON_CreateObject();
    cJSON *BottomRight  = cJSON_CreateObject();

    cJSON *DetectTargetList = cJSON_CreateArray();

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    int i = 0;
    int ID = 0;

    cJSON_AddNumberToObject(p1, "ID", ID);
    cJSON_AddNumberToObject(p1, "Enabled", pstPdAlarm->enable);
    if (pstPdAlarm->threshold == 75 || pstPdAlarm->threshold > 10)
        pstPdAlarm->threshold = 0;

    cJSON_AddNumberToObject(p1, "Sensitivity", (pstPdAlarm->sensitivity * 10) + pstPdAlarm->threshold);
    if(1)
    {
        cJSON_AddNumberToObject(TopLeft, "X", 0);
        cJSON_AddNumberToObject(TopLeft, "Y", 0);
        cJSON_AddNumberToObject(BottomRight, "X", 10000);
        cJSON_AddNumberToObject(BottomRight, "Y", 10000);
        cJSON_AddItemToObject(RECTArea, "TopLeft", TopLeft);
        cJSON_AddItemToObject(RECTArea, "BottomRight", BottomRight);
        cJSON_AddItemToObject(p1, "RECTArea", RECTArea);
    }

    if(pstPdAlarm->polygonArea.count >= 3)
    {
        int PointNum = pstPdAlarm->polygonArea.count;
        cJSON_AddNumberToObject(AreaInfoList, "PointNum", PointNum);
        if (PointNum > 0)
        {
            for(i = 0; i < PointNum; i++)
            {
                cJSON *XY = cJSON_CreateObject();
                cJSON_AddNumberToObject(XY, "X", (pstPdAlarm->polygonArea.points[i].x)*100);
                cJSON_AddNumberToObject(XY, "Y", (pstPdAlarm->polygonArea.points[i].y)*100);
                cJSON_AddItemToArray(PointList, XY);    
            }
            cJSON_AddItemToObject(AreaInfoList, "PointList", PointList);
        }
        cJSON_AddItemToObject(p1, "AreaInfoList", AreaInfoList);
    }

    cJSON_AddNumberToObject(p1, "Num", 3);          ///人2非1车0

    cJSON *DetectTargetInfo_car0 = NULL;
    alarm_smart_motion_target_info_get(pstPdAlarm, &DetectTargetInfo_car0, 0);  // 机动车
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_car0);

    cJSON *DetectTargetInfo_car1 = NULL;
    alarm_smart_motion_target_info_get(pstPdAlarm, &DetectTargetInfo_car1, 1);  // 非机动车
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_car1);

    cJSON *DetectTargetInfo_car2 = NULL;
    alarm_smart_motion_target_info_get(pstPdAlarm, &DetectTargetInfo_car2, 2);  // 人形
    cJSON_AddItemToArray(DetectTargetList, DetectTargetInfo_car2);

    cJSON_AddItemToObject(p1, "DetectTargetList", DetectTargetList);

    *p = p1;
    return 0;
}

int unv_alarm_smart_motion_linkage_action_get(cJSON **p)
{
    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

    if (1)
    {
        int Num = 0;
        cJSON *LinkageActionList = cJSON_CreateObject();
        cJSON *Actions = cJSON_CreateArray();

        /////////////////////////////////////////////////////音频
        if (0)
        {
            Num++;
            cJSON *AudioActParamInfo = NULL;
            alarm_motion_link_audio_alarm_info_get(pstPdAlarm, &AudioActParamInfo, 0);
            cJSON_AddItemToArray(Actions, AudioActParamInfo);
        }
        if (1)
        {
            Num++;
            cJSON *AudioLinkagePlanInfo = NULL;
            alarm_motion_link_audio_plan_info_get(pstPdAlarm, &AudioLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, AudioLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////

        /////////////////////////////////////////////////////灯光
        if (0)
        {
            Num++;
            cJSON *LightActParamInfo = NULL;
            alarm_motion_link_light_alarm_info_get(pstPdAlarm, &LightActParamInfo, 0);
            cJSON_AddItemToArray(Actions, LightActParamInfo);
        }
        if (1)
        {
            Num++;
            cJSON *LightLinkagePlanInfo = NULL;
            alarm_motion_link_light_plan_info_get(pstPdAlarm, &LightLinkagePlanInfo);
            cJSON_AddItemToArray(Actions, LightLinkagePlanInfo);
        }
        /////////////////////////////////////////////////////

        cJSON_AddNumberToObject(LinkageActionList, "Num", Num);
        cJSON_AddItemToObject(LinkageActionList, "Actions", Actions);

        *p = LinkageActionList;
    }
    else
    {
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        AudioCapture *pstAudioCaptureCfg = &pstMediaCfg->audioConfig.audioCapture;

        cJSON *LinkageActionList = cJSON_CreateObject();
        cJSON *Actions = cJSON_CreateArray();
        cJSON *LinkageActionInfo = cJSON_CreateObject();
        cJSON *ActParam = cJSON_CreateObject();

        if (pstPdAlarm->alarmAction.audioAction.enable.enable_flag == ARMING_DISABLE)
            cJSON_AddNumberToObject(ActParam, "Enabled", 0);
        else
            cJSON_AddNumberToObject(ActParam, "Enabled", 1);
        cJSON_AddNumberToObject(ActParam, "AudioFileID", 1);

        if (pstPdAlarm->alarmAction.audioAction.times >= 0)
            cJSON_AddNumberToObject(ActParam, "WarnCount", pstPdAlarm->alarmAction.audioAction.times);
        else
            cJSON_AddNumberToObject(ActParam, "WarnCount", 50);

        float vol_val = (float)pstAudioCaptureCfg->volume_play;
        vol_val = vol_val/100*255;
        cJSON_AddNumberToObject(ActParam, "AudioVolume", (int)vol_val);

        cJSON_AddNumberToObject(LinkageActionInfo, "ActID", UNV_ACT_ID_AUDIO_ALARM);
        cJSON_AddItemToObject(LinkageActionInfo, "ActParam", ActParam);
        cJSON_AddItemToArray(Actions, LinkageActionInfo);

        cJSON_AddNumberToObject(LinkageActionList, "Num", 1);
        cJSON_AddItemToObject(LinkageActionList, "Actions", Actions);

        *p = LinkageActionList;
    }

    return 0;
}

//-------------------------------------------------------------------------------------------
//                          alarm ability
//-------------------------------------------------------------------------------------------

void alarm_capability_str_get(char *str, int len)
{
    snprintf(str, len, unv_str_alarm_capability, 1, 1);    
}

int unv_alarm_capability_get(cJSON **p)
{
    int ability_buf_len = 1024;
    char *pAbilityStr = (char *)anj_mw_malloc(ability_buf_len);
    if (pAbilityStr == NULL)
    {
        __ERR("pAbilityStr malloc failed\n");
        return -1;
    }

    alarm_capability_str_get(pAbilityStr, ability_buf_len);

    cJSON *jsonbuf = cJSON_Parse(pAbilityStr);
    if (jsonbuf == NULL)
    {
        anj_mw_free(pAbilityStr);
        return -1;
    }
    
    anj_mw_free(pAbilityStr);
    pAbilityStr = NULL;
    
    *p = jsonbuf;
    return 0;
}

//-------------------------------------------------------------------------------------------
//                          smart ability
//-------------------------------------------------------------------------------------------

int unv_smart_working_status_info_get(cJSON **p)
{
    cJSON *WorkingStatusInfo = cJSON_CreateObject();
    if (WorkingStatusInfo == NULL)
    {
        __ERR("cJSON_CreateObject WorkingStatusInfo failed\n");
        return -1;
    }

    cJSON *EnableIDs    = cJSON_CreateArray();
    cJSON *DisableIDs   = cJSON_CreateArray();
    int EnableNum = 0;
    int DisableNum = 0;

    int chn = 0;
    AlarmConfig *pstAlarmCfg = (AlarmConfig *)getAlarmConfig();
    http_alarm_ability_t *pstAbility = http_alarm_ability_get();

    if (pstAbility->smart_motion)   /*智能*/
    {
        PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

        cJSON *Smart = cJSON_CreateNumber(UNV_SMART_ID_SMART_MOTION);
        if (pstPdAlarm->enable)
        {
            cJSON_AddItemToArray(EnableIDs, Smart);
            EnableNum ++;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, Smart);
            DisableNum ++;
        }
    }

    if (pstAbility->video_gate)     /*越界*/
    {
        VideoGateAlarm *pstVideoGateAlarm = &pstAlarmCfg->aiAlarm.vgAlarm[chn];
        cJSON *CrossLineJson = cJSON_CreateNumber(UNV_SMART_ID_CROSS_LINE);
        if (pstVideoGateAlarm->enable)
        {
            cJSON_AddItemToArray(EnableIDs, CrossLineJson);
            EnableNum ++;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, CrossLineJson);
            DisableNum ++;
        }
    }

    if (pstAbility->region_enable)  /*区域*///区域检测:区域入侵、进入区域、离开区域
    {
        VideoRegionAiAlarm *pstRegionAlarm = &pstAlarmCfg->aiAlarm.regionAiAlarm[chn];
        cJSON *Its0 = cJSON_CreateNumber(UNV_SMART_ID_INTRUSION);
        cJSON *Its1 = cJSON_CreateNumber(UNV_SMART_ID_ACCESS_ZONE);
        cJSON *Its2 = cJSON_CreateNumber(UNV_SMART_ID_LEAVE_ZONE);

        if (pstRegionAlarm->data[0].enable)
        {
            cJSON_AddItemToArray(EnableIDs, Its0);
            EnableNum += 1;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, Its0);
            DisableNum += 1;
        }
        if (pstRegionAlarm->data[1].enable)
        {
            cJSON_AddItemToArray(EnableIDs, Its1);
            EnableNum += 1;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, Its1);
            DisableNum += 1;
        }
        if (pstRegionAlarm->data[2].enable)
        {
            cJSON_AddItemToArray(EnableIDs, Its2);
            EnableNum += 1;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, Its2);
            DisableNum += 1;
        }
    }

    if (pstAbility->face_detect)        /*人脸*/
    {
        FaceDetectAlarm *pstFdAlarm = &pstAlarmCfg->aiAlarm.fdAlarm[chn];
        cJSON *Fac = cJSON_CreateNumber(UNV_SMART_ID_FACE_DETECT);
        if (pstFdAlarm->enable)
        {
            cJSON_AddItemToArray(EnableIDs, Fac);
            EnableNum ++;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, Fac);
            DisableNum ++;
        }
    }

    if (pstAbility->auto_track)    // 跟踪
    {
        PdAlarm *pstPdAlarm = &pstAlarmCfg->aiAlarm.pdAlarm[chn];

        cJSON *TrackingJson = cJSON_CreateNumber(UNV_SMART_ID_AUTO_TRACK);       // 业务ID 3
        if (pstPdAlarm->enable && pstPdAlarm->alarmAction.track_human_enable)
        {
            cJSON_AddItemToArray(EnableIDs, TrackingJson);
            EnableNum ++;
        }
        else
        {
            cJSON_AddItemToArray(DisableIDs, TrackingJson);
            DisableNum ++;
        }
    }

    cJSON_AddNumberToObject(WorkingStatusInfo, "EnableNum", EnableNum);
    cJSON_AddItemToObject(WorkingStatusInfo, "EnableIDs", EnableIDs);
    cJSON_AddNumberToObject(WorkingStatusInfo, "DisableNum", DisableNum);
    cJSON_AddItemToObject(WorkingStatusInfo, "DisableIDs", DisableIDs);
    
    *p = WorkingStatusInfo;
    return 0;
}

int unv_smart_mutex_relation_info_get(cJSON **p)
{
    int i = 0;
    cJSON *MutexRelationInfos = cJSON_CreateObject();
    if (MutexRelationInfos == NULL)
    {
        __ERR("cJSON_CreateObject MutexRelationInfos failed\n");
        return -1;
    }

    cJSON *CompatibleRelationsArray = cJSON_CreateArray();
    //直接加,不管有没有该智能,该接口值说明互斥
    if (1)//g_hasSMART)//智能
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Smart = cJSON_CreateNumber(UNV_SMART_ID_SMART_MOTION);
        cJSON_AddItemToArray(IDs1, Smart);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }

    if (1)//g_hasGATE)//有越界
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Cld = cJSON_CreateNumber(UNV_SMART_ID_CROSS_LINE);
        cJSON_AddItemToArray(IDs1, Cld);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }
    if (1)//g_hasREGION)//区域入侵//进入区域//离开区域
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 3);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Its1 = cJSON_CreateNumber(UNV_SMART_ID_INTRUSION);
        cJSON *Its2 = cJSON_CreateNumber(UNV_SMART_ID_ACCESS_ZONE);
        cJSON *Its3 = cJSON_CreateNumber(UNV_SMART_ID_LEAVE_ZONE);
        cJSON_AddItemToArray(IDs1, Its1);
        cJSON_AddItemToArray(IDs1, Its2);
        cJSON_AddItemToArray(IDs1, Its3);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }
    if (0)//g_hasREGION)//进入区域
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Its = cJSON_CreateNumber(UNV_SMART_ID_ACCESS_ZONE);
        cJSON_AddItemToArray(IDs1, Its);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }
    if (0)//g_hasREGION)//离开区域
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Its = cJSON_CreateNumber(UNV_SMART_ID_LEAVE_ZONE);
        cJSON_AddItemToArray(IDs1, Its);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }
    if (1)//g_hasFACE)//人脸
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Fac = cJSON_CreateNumber(UNV_SMART_ID_FACE_DETECT);
        cJSON_AddItemToArray(IDs1, Fac);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }
    if (1)//g_hasTracking)//跟踪
    {
        i ++;
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Fac = cJSON_CreateNumber(UNV_SMART_ID_AUTO_TRACK);
        cJSON_AddItemToArray(IDs1, Fac);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }

    for (; i < 8; i++)
    {
        cJSON *CompatibleRelationsArray1 = cJSON_CreateObject();
        cJSON_AddNumberToObject(CompatibleRelationsArray1, "Num", 1);
        cJSON *IDs1 = cJSON_CreateArray();
        cJSON *Fac = cJSON_CreateNumber(UNV_SMART_ID_FIX_DETECT);
        cJSON_AddItemToArray(IDs1, Fac);
        cJSON_AddItemToObject(CompatibleRelationsArray1, "IDs", IDs1);
        cJSON_AddItemToArray(CompatibleRelationsArray, CompatibleRelationsArray1);
    }

    cJSON_AddNumberToObject(MutexRelationInfos, "Num", 8);
    cJSON_AddItemToObject(MutexRelationInfos, "CompatibleRelations", CompatibleRelationsArray);

    *p = MutexRelationInfos;
    return 0;
}

int unv_smart_capability_get(cJSON **p)
{
    http_alarm_ability_t *pstAbility = http_alarm_ability_get();

    cJSON *SmartCapInfoJson = cJSON_CreateObject();
    if (SmartCapInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject SmartCapInfoJson failed\n");
        return -1;
    }

    if (pstAbility->smart_motion)           //智能移动
    {
        cJSON *SmartMotionDetectionCapInfo = NULL;
        alarm_smart_motion_capability_info_ex_get(pstAbility->smart_motion, &SmartMotionDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "SmartMotionDetection", SmartMotionDetectionCapInfo);
    }

    if (pstAbility->car_enable)           //Vehicle//车型
    {
        cJSON *VehicleDetectionCapInfo = NULL;
        smart_vehicle_capability_info_get(pstAbility->car_enable, &VehicleDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "VehicleDetection", VehicleDetectionCapInfo);
    }

    if (pstAbility->video_gate)         //越界
    {
        cJSON *CrossLineDetectionCapInfo = NULL;
        smart_crossline_capability_info_get(pstAbility->video_gate, pstAbility->auto_track, &CrossLineDetectionCapInfo);/*越界暂不支持*/
        cJSON_AddItemToObject(SmartCapInfoJson, "CrossLineDetection", CrossLineDetectionCapInfo);/*越界暂不支持*/
    }

    if (pstAbility->region_enable)       //区域
    {
        cJSON *IntrusionDetectionCapInfo = NULL;
        smart_intrusion_capability_info_get(pstAbility->region_enable, &IntrusionDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "IntrusionDetection", IntrusionDetectionCapInfo);

        cJSON *AccessZoneCapInfo = cJSON_Duplicate(IntrusionDetectionCapInfo, 1);
        //smart_intrusion_capability_info_get(&AccessZoneCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "AccessZone", AccessZoneCapInfo);

        cJSON *LeaveZoneCapInfo = cJSON_Duplicate(IntrusionDetectionCapInfo, 1);
        //smart_intrusion_capability_info_get(&LeaveZoneCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "LeaveZone", LeaveZoneCapInfo);
    }

    if (pstAbility->face_detect)    //人脸
    {
        cJSON *FaceDetectionCapInfo = NULL;
        smart_facedetect_capability_info_get(pstAbility->face_detect, &FaceDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "FaceDetection", FaceDetectionCapInfo);
    }

    if (pstAbility->auto_track)
    {
        cJSON *TrackingCapInfo = NULL;
        smart_autotrack_capability_info_get(pstAbility->auto_track, &TrackingCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "ObjTrack", TrackingCapInfo);
    }

    *p = SmartCapInfoJson;
    return 0;
}

int unv_smart_capability_ex_get(cJSON **p)
{
    http_alarm_ability_t *pstAbility = http_alarm_ability_get();

    cJSON *SmartCapInfoJson = cJSON_CreateObject();
    if (SmartCapInfoJson == NULL)
    {
        __ERR("cJSON_CreateObject SmartCapInfoJson failed\n");
        return -1;
    }

    if (pstAbility->smart_motion)       //智能
    {
        cJSON *SmartMotionDetectionCapInfo = NULL;
        alarm_smart_motion_capability_info_ex_get(pstAbility->smart_motion, &SmartMotionDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "SmartMotionDetection", SmartMotionDetectionCapInfo);
    }

    if (pstAbility->video_gate)         //越界
    {
        cJSON *CrossLineDetectionCapInfo = NULL;
        smart_crossline_capability_info_get(pstAbility->video_gate, pstAbility->auto_track, &CrossLineDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "CrossLineDetection", CrossLineDetectionCapInfo);
    }

    if (pstAbility->region_enable)      //区域
    {
        cJSON *IntrusionDetectionCapInfo = NULL;
        smart_intrusion_capability_info_get(pstAbility->region_enable, &IntrusionDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "IntrusionDetection", IntrusionDetectionCapInfo);
        
        cJSON *AccessZoneCapInfo = NULL;
        smart_intrusion_capability_info_get(pstAbility->region_enable, &AccessZoneCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "AccessZone", AccessZoneCapInfo);
        
        cJSON *LeaveZoneCapInfo = NULL;
        smart_intrusion_capability_info_get(pstAbility->region_enable, &LeaveZoneCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "LeaveZone", LeaveZoneCapInfo);
    }

    if (pstAbility->face_detect)        //人脸
    {
        cJSON *FaceDetectionCapInfo = NULL;
        smart_facedetect_capability_info_get(pstAbility->face_detect, &FaceDetectionCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "FaceDetection", FaceDetectionCapInfo);
    }

    if (pstAbility->auto_track)         // 自动跟踪
    {
        cJSON *TrackingCapInfo = NULL;
        smart_autotrack_capability_info_get(pstAbility->auto_track, &TrackingCapInfo);
        cJSON_AddItemToObject(SmartCapInfoJson, "ObjTrack", TrackingCapInfo);
    }

    *p = SmartCapInfoJson;
    return 0;
}

