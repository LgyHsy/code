#include <unistd.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdarg.h>

#include "anj_mw_comm.h"
#include "anj_module.h"
#include "anj_bind.h"
#include "anj_config.h"
#include "anj_audio.h"
#include "anj_record.h"
#include "eventhub.h"

#include "sk_def.h"
#include "skyapi_soundwave.h"

static int s_stWaveRun = 0;

/* 声波配网回调函数 */
sk_status_code_t wireless_soundwave_callback(const sky_wireless_soundwave_callback_data_t *p_data)
{
    sk_status_code_t status = SK_FAILED;

    if (p_data == NULL)
        return status;

    if (p_data->status == SK_WIRELESS_SOUNDWAVE_STATUS_GETTING)
    {
        __INFO("SK_WIRELESS_SOUNDWAVE_STATUS_GETTING\n");
        status = SK_SUCCESS;
    }
    else if (p_data->status == SK_WIRELESS_SOUNDWAVE_STATUS_SUCCESS)
    {
        __INFO("SK_WIRELESS_SOUNDWAVE_STATUS_SUCCESS conf:%s \n", p_data->conf);
        anj_bind_data_proc(p_data->conf, sizeof(p_data->conf), BIND_TYPE_WAVE);
    }
    else if (p_data->status == SK_WIRELESS_SOUNDWAVE_STATUS_FAILED)
    {
        status = SK_FAILED;
        __ERR("SK_WIRELESS_SOUNDWAVE_STATUS_FAILED\n");
    }

    return status;
}

static void anj_wave_send_data(EventResult *event_result, void *data)
{
    event_data_s *pstEventData = (event_data_s *)data;
    if (s_stWaveRun && pstEventData->data && pstEventData->len > 0)
    {
        if (skyapi_wireless_is_soundwave_full())
        {
            __ERR("skyapi_wireless_is_soundwave_full \n");
            return;
        }
        sk_status_code_t statuscode = skyapi_wireless_soundwave_voicein(pstEventData->data, pstEventData->len);
        if (statuscode != SK_SUCCESS)
        {
            __ERR("voice in error, statuscode=%d\n", statuscode);
        }
    }
}

static void anj_wave_status_set(EventResult *event_result, void *data)
{
    if (data)
    {
        int resetAudio = 0;
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        AudioCapture *pstAudioCapture = &pstMediaCfg->audioConfig.audioCapture;
        static int oriSamplerate = pstAudioCapture->samplerate;
        s_stWaveRun = *(int *)data;
        __INFO("s_stWaveRun:%d\n", s_stWaveRun);
        if (s_stWaveRun)
        {
            if (pstAudioCapture->samplerate != 16000)
            {
                resetAudio = 1;
                pstAudioCapture->samplerate = 16000;
            }
            skyapi_wireless_soundwave_start();
        }
        else
        {
            if (pstAudioCapture->samplerate != oriSamplerate)
            {
                resetAudio = 1;
                pstAudioCapture->samplerate = oriSamplerate;
            }
            skyapi_wireless_soundwave_stop();
        }
        if (resetAudio)
        {
            anj_audio_restart();
            anj_record_restart();
        }
    }
}

static void anj_wave_status_get(EventResult *event_result, void *data)
{
    if (event_result)
    {
        event_result->ret = s_stWaveRun;
    }
}


/*****************************************************************************
 函 数 名  : anj_wave_init
 功能描述  : 声波识别绑定初始化
 输入参数  :
 输出参数  :
 返 回 值  :  0成功，其他失败
*****************************************************************************/
int anj_wave_init()
{
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WAVE_SEND_DATA, anj_wave_send_data);
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WAVE_SET_STATUS, anj_wave_status_set);
    eventhub_subscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WAVE_GET_STATUS, anj_wave_status_get);

    skyapi_wireless_soundwave_register_callback(wireless_soundwave_callback);
    return 0;
}

/*****************************************************************************
 函 数 名  : anj_wave_uninit
 功能描述  : 声波识别绑定退出
 输入参数  :   无
 输出参数  :
 返 回 值  :  0成功，其他失败
*****************************************************************************/
int anj_wave_uninit()
{
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WAVE_SEND_DATA, anj_wave_send_data);
    eventhub_unsubscribe(EVENTHUB_CLASS_MEDIA, (char *)EVENTHUB_WAVE_SET_STATUS, anj_wave_status_set);
    return 0;
}

REGISTER_MODULE(anj_wave, MODULE_PRIORITY_WAVE);
