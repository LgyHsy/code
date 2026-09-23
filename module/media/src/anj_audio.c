#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_mem.h"
#include "anj_mw_thread.h"
#include "anj_mw_hwctrl.h"
#include "anj_mw_file.h"
#include "anj_mw_errcode.h"

#include "anj_module.h"
#include "eventhub.h"
#include "anj_audio.h"
#include "anj_mbuf.h"
#include "anj_ser.h"
#include "anj_service.h"
#include "anj_sysmng.h"
#include "anj_mw_media_audio.h"
#include "anj_config.h"
#include "anj_net.h"
#include "audio_utils.h"
#include "audio_receiver.h"

#define AUDIO_AAC_PERIOD_SIZE (1024)
#define AUDIO_AIAO_TEST_PLAY_DELAY_MS 2500   // AI AO采集音频最多2.5s
#define AUDIO_AIAO_TEST_BUF_SIZE (40 * 1024) // AI AO采集音频buf最大40k
#define AUDIO_AO_END_TIME 1500               // 1.5s结束没有ao输出 代表ao状态关闭

typedef struct
{
    audioplay_info current;          // 当前正在播放的任务
    audioplay_info next;             // 下一个播放的任务
    AudioFilePlayStatus play_status; // 文件播放状态
    int has_new_request;             // 是否有新播放请求
} audio_play_file_manager_t;

static int s_stAudioInit = 0;
static AnjAudioConfig gstAnjAudioCfg;
static void *pstEncHandle = NULL;
FILE *pFile = NULL;
FILE *pFile1 = NULL;
FILE *pFile2 = NULL;

// ao播放状态
static AudioPlayInfo s_stAudioPlayInfo = {0};
static pthread_mutex_t s_stAudioPlayInfoMutex = PTHREAD_MUTEX_INITIALIZER;

// ai ao测试信息
static AudioAiAoTestInfo_t s_stAudioAiAoTestInfo = {0};
static pthread_mutex_t s_stAudioAiAoTestMutex = PTHREAD_MUTEX_INITIALIZER;
static anj_thread_s s_stAiaoTestPlayThread = {0};
static volatile int s_aiao_test_play_scheduled = 0;
static audio_play_file_manager_t s_stAudioPlayFileManager = {0};
static anj_thread_s s_stAudioAoPlayThread;
static pthread_mutex_t s_gAudioPlayFileMutex = PTHREAD_MUTEX_INITIALIZER;

// 音频对讲相关
static void *pTalkDecHandle = NULL;
static media_codec_type_e s_eTalkDecSrcCodec = MEDIA_CODEC_NONE;
static int s_iTalkDecSrcSampleRate = 0;
static int s_iTalkDecBTalk = 0;

static void anj_audio_play_pcm(char *data, int data_len, int bTalk)
{
    int write_len = 0;
    int frame_len = 0;
    int remain_len = data_len;
    char *audio_frame = NULL;

    if (s_stAudioInit && data && data_len > 0)
    {
        anj_mutex_lock(&s_stAudioPlayInfoMutex);
        s_stAudioPlayInfo.bTalk = bTalk;
        if (s_stAudioPlayInfo.playtime == 0)
        {
            anj_mw_hwctrl_switch_ao_open();
        }
        s_stAudioPlayInfo.playtime = anj_mw_get_cputime_ms(NULL);
        anj_mutex_unlock(&s_stAudioPlayInfoMutex);

        while (remain_len > 0)
        {
            frame_len = (remain_len > AUDIO_PERIOD_SIZE) ? AUDIO_PERIOD_SIZE : remain_len;
            audio_frame = data + write_len;

            write_len += frame_len;
            remain_len -= frame_len;

            anj_mw_media_audio_play(audio_frame, frame_len);

            anj_mutex_lock(&s_stAudioPlayInfoMutex);
            s_stAudioPlayInfo.playtime = anj_mw_get_cputime_ms(NULL);
            anj_mutex_unlock(&s_stAudioPlayInfoMutex);
        }
    }
}

static int anj_audio_aiao_test_play_thread(void *ctx, int *bStart)
{
    char *play_buf = NULL;
    int play_len = 0;
    int mic_samplerate = 8000;

    (void)ctx;

    anj_mutex_lock(&s_stAudioAiAoTestMutex);
    if (s_stAudioAiAoTestInfo.buff != NULL && s_stAudioAiAoTestInfo.buf_pos > 0)
    {
        play_len = s_stAudioAiAoTestInfo.buf_pos;
        play_buf = (char *)anj_mw_malloc(play_len);
        if (play_buf != NULL)
        {
            memcpy(play_buf, s_stAudioAiAoTestInfo.buff, play_len);
        }
    }
    anj_mutex_unlock(&s_stAudioAiAoTestMutex);

    if (play_buf != NULL && play_len > 0)
    {
        MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
        media_codec_type_e mic_codec = MEDIA_CODEC_AUDIO_PCM;

        mic_samplerate = pstMediaCfg->audioConfig.audioCapture.samplerate;
        if (mic_samplerate <= 0)
        {
            mic_samplerate = 8000;
        }

        __INFO("aiao test play, len=%d samplerate=%d\n", play_len, mic_samplerate);
        anj_audio_play_data(play_buf, play_len, 1, mic_codec, mic_samplerate);
    }

    if (play_buf != NULL)
    {
        anj_mw_free(play_buf);
    }

    anj_audio_aiao_test_stop();
    s_aiao_test_play_scheduled = 0;

    if (bStart)
    {
        *bStart = 0;
    }

    return 0;
}

static void anj_audio_aiao_test_schedule_playback(void)
{
    if (s_aiao_test_play_scheduled)
    {
        return;
    }

    if (s_stAiaoTestPlayThread.start != 0 && s_stAiaoTestPlayThread.end == 0)
    {
        return;
    }

    s_aiao_test_play_scheduled = 1;
    memset(&s_stAiaoTestPlayThread, 0, sizeof(s_stAiaoTestPlayThread));
    s_stAiaoTestPlayThread.bAutoDestroy = 1;
    strncpy(s_stAiaoTestPlayThread.iThreadName, "aiao_test_play",
            sizeof(s_stAiaoTestPlayThread.iThreadName) - 1);
    s_stAiaoTestPlayThread.iThreadjob.ctx = &s_stAiaoTestPlayThread;
    s_stAiaoTestPlayThread.iThreadjob.func = anj_audio_aiao_test_play_thread;
    anj_thread_task_create(&s_stAiaoTestPlayThread);
}

static void anj_audio_aiao_handle(char *data, int len, unsigned long long int timestamp)
{
    int need_play = 0;

    if (data == NULL || len <= 0)
    {
        return;
    }

    anj_mutex_lock(&s_stAudioAiAoTestMutex);
    if (s_stAudioAiAoTestInfo.startup)
    {
        if (s_stAudioAiAoTestInfo.finish_status == 0 && s_stAudioAiAoTestInfo.buff != NULL)
        {
            if (s_stAudioAiAoTestInfo.start_pts == 0)
            {
                s_stAudioAiAoTestInfo.start_pts = timestamp;
            }

            // 采集2.5s的音频数据或者数据量达到40kb
            if ((timestamp - s_stAudioAiAoTestInfo.start_pts) <= AUDIO_AIAO_TEST_PLAY_DELAY_MS * 1000 &&
                (s_stAudioAiAoTestInfo.buf_pos + len) <= AUDIO_AIAO_TEST_BUF_SIZE)
            {
                memcpy(s_stAudioAiAoTestInfo.buff + s_stAudioAiAoTestInfo.buf_pos, data, len);
                s_stAudioAiAoTestInfo.buf_pos = s_stAudioAiAoTestInfo.buf_pos + len;
            }
            else
            {
                s_stAudioAiAoTestInfo.finish_status = 1;
                need_play = 1;
            }
        }
    }

    anj_mutex_unlock(&s_stAudioAiAoTestMutex);

    if (need_play)
    {
        anj_audio_aiao_test_schedule_playback();
    }
}

static void anj_audio_adec_data_cb(void *user, char *data, int len, unsigned long long int timestamp, unsigned int seq)
{
    if (s_stAudioInit && data && (len > 0))
    {
        if (0 == access("/tmp/adec", F_OK))
        {
            if (pFile2 == NULL)
                pFile2 = fopen("/tmp/nfs/adec.pcm", "wb");
        }
        else
        {
            if (pFile2)
            {
                fclose(pFile2);
                pFile2 = NULL;
            }
        }

        if (pFile2)
        {
            fwrite(data, 1, len, pFile2);
        }

        anj_audio_play_pcm(data, len, s_iTalkDecBTalk);
    }
}

static void anj_audio_ao_close_check(void)
{
    if (s_stAudioInit == 0)
        return;
    int ao_play_data = 1;
    static int check_times = 0;

    anj_mutex_lock(&s_stAudioPlayInfoMutex);
    if ((s_stAudioPlayInfo.playtime > 0) &&
        ((anj_mw_get_cputime_ms(NULL) - s_stAudioPlayInfo.playtime) > AUDIO_AO_END_TIME))
    {
        check_times++;

        // 因为 get ai_stream没有用usleep控制，大概1秒执行13~15次，这里差不多每700ms检测一次ao是否还有数据
        if ((check_times % 10) == 0)
        {
            ao_play_data = anj_mw_media_audio_ao_play_check();
        }

        if (ao_play_data == 0)
        {
            s_stAudioPlayInfo.playtime = 0;
            s_stAudioPlayInfo.bTalk = 0;
            anj_mw_hwctrl_switch_ao_close();
        }
    }
    else
    {
        check_times = 0;
    }

    anj_mutex_unlock(&s_stAudioPlayInfoMutex);
}

static int anj_audio_do_play_file(audioplay_info *pstAudioInfo)
{
    int iRet = -1;
    char *pBuffer = NULL;

    if (!pstAudioInfo)
    {
        return -1;
    }

    unsigned long long int flen = 0;
    if (0 != anj_mw_read_file_len(pstAudioInfo->pAudioFile, &flen))
    {
        __ERR("read file %s err!\n", pstAudioInfo->pAudioFile);
        goto endFunc;
    }

    pBuffer = (char *)anj_mw_malloc(flen);
    ANJ_CHK(pBuffer != NULL, -1, "malloc failed!");
    memset(pBuffer, 0, flen);

    if (0 != anj_mw_read_file(pstAudioInfo->pAudioFile, pBuffer, &flen))
    {
        __ERR("read file %s err!\n", pstAudioInfo->pAudioFile);
        goto endFunc;
    }

    int srcSampleRate = gstAnjAudioCfg.ao_samplerate;
    int srcChn = gstAnjAudioCfg.chn;
    int dstSampleRate = gstAnjAudioCfg.ao_samplerate;
    int dstChn = gstAnjAudioCfg.chn;
    media_codec_type_e srcEncode = pstAudioInfo->encodeType;

    int i = 0;
    int ori_playtimes = pstAudioInfo->playtimes;
    for (i = 0; i < ori_playtimes; i++)
    {
        anj_mutex_lock(&s_gAudioPlayFileMutex);
        int cur_playtimes = s_stAudioPlayFileManager.current.playtimes;
        anj_mutex_unlock(&s_gAudioPlayFileMutex);

        int play_pri_interrupt = 0;
        if (cur_playtimes == 1 && cur_playtimes != ori_playtimes)
        {
            __INFO("audio play this time and end!\n");
            play_pri_interrupt = 1;
        }

        s_iTalkDecBTalk = 0; // 播放文件时不是对讲，重置为0
        void *pstDecHandle = AU_Create(NULL, srcEncode, srcSampleRate, srcChn,
                                       MEDIA_CODEC_AUDIO_PCM, dstSampleRate, dstChn,
                                       AUDIO_PERIOD_SIZE, anj_audio_adec_data_cb);
        ANJ_CHK(pstDecHandle != NULL, -1, "AU_Create failed!");

        AU_Codec(pstDecHandle, srcEncode, MEDIA_CODEC_AUDIO_PCM, (uint8_t *)pBuffer, flen, 0, 0);
        AU_Destroy(pstDecHandle, srcEncode, MEDIA_CODEC_AUDIO_PCM);

        if (play_pri_interrupt == 1)
        {
            break;
        }

        if (i < ori_playtimes - 1)
        {
            usleep(1 * 1000 * 1000);
        }
    }

    iRet = 0;

endFunc:
    if (pBuffer)
    {
        anj_mw_free(pBuffer);
        pBuffer = NULL;
    }

    return iRet;
}

static void anj_audio_pcm_data_cb(char *data, int len, unsigned long long int timestamp, unsigned int u32Seq)
{
    if (s_stAudioInit == 0)
        return;
    if (data && (len > 0))
    {
        if (0 == access("/tmp/audio", F_OK))
        {
            if (pFile == NULL)
                pFile = fopen("/tmp/nfs/audio.pcm", "wb");
        }
        else
        {
            if (pFile)
            {
                fclose(pFile);
                pFile = NULL;
            }
        }
        if (pFile)
        {
            fwrite(data, 1, len, pFile);
        }

        anj_audio_ao_close_check();

        EventResult event_result = {0};
        eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_GET_STATUS, &event_result, NULL);
        char *pcm_data = data;
        int pcm_len = len;

        if (event_result.ret == 1)
        {
            EventResult data_event_result = {0};
            event_data_s event_data = {0};
            event_data.data = data;
            event_data.len = len;
            eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_SEND_DATA, &data_event_result, (event_data_s *)&event_data);
        }
        else
        {
            anj_mw_media_audio_ai_pcm_process(data, len, gstAnjAudioCfg.chn, &gstAnjAudioCfg,
                                              &pcm_data, &pcm_len);

            anj_audio_aiao_handle(pcm_data, pcm_len, timestamp);
        }

        if (gstAnjAudioCfg.aenc_enable && pstEncHandle)
        {
            AU_Codec(pstEncHandle, MEDIA_CODEC_AUDIO_PCM, gstAnjAudioCfg.encodeType, (uint8_t *)pcm_data, pcm_len, timestamp, u32Seq);
        }
    }
}

static void anj_audio_aenc_data_cb(void *user, char *data, int len, unsigned long long int timestamp, unsigned u32Seq)
{
    if (s_stAudioInit == 0)
        return;
    if (data && (len > 0))
    {
        if (0 == access("/tmp/aenc", F_OK))
        {
            if (pFile1 == NULL)
                pFile1 = fopen("/tmp/nfs/audio.pcm", "wb");
        }
        else
        {
            if (pFile1)
            {
                fclose(pFile1);
                pFile1 = NULL;
            }
        }
        if (pFile1)
        {
            fwrite(data, 1, len, pFile1);
        }

        media_frame_info_t stFrameInfo = {0};
        stFrameInfo.frameBuf = (unsigned char *)data;
        stFrameInfo.frameParam.frameLen = len;
        stFrameInfo.frameParam.aframeIndex = u32Seq;
        stFrameInfo.frameParam.frameKeyIndex = stFrameInfo.frameParam.aframeIndex;
        stFrameInfo.frameParam.frameType = MEDIA_AFRAME_A;
        stFrameInfo.frameParam.frameCodec = gstAnjAudioCfg.encodeType;
        stFrameInfo.frameParam.framePts = timestamp / 1000;
        stFrameInfo.frameParam.frameTime = time(NULL);

#if 0
        __INFO("Get frame(%u) %d. pts:%llu,%lu\n", stFrameInfo.frameParam.aframeIndex,
                    stFrameInfo.frameParam.frameLen, stFrameInfo.frameParam.framePts, stFrameInfo.frameParam.frameTime);
#endif
        anj_mbuf_audio_write_frame(&stFrameInfo);
    }
}

static int anj_audio_attr_init(AnjAudioConfig *pstAnjAudioCfg)
{
    int encPersize = AUDIO_PERIOD_SIZE >> 1;
    MediaConfig *pstMediaCfg = (MediaConfig *)getMediaConfig();
    AudioConfig stAudioCfg = {0};
    memcpy(&stAudioCfg, &pstMediaCfg->audioConfig, sizeof(AudioConfig));
    memset(pstAnjAudioCfg, 0, sizeof(AnjAudioConfig));

    if ((strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "PCMA") == 0) ||
        (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G711A") == 0) ||
        (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G.711A") == 0))
    {
        pstAnjAudioCfg->encodeType = MEDIA_CODEC_AUDIO_G711A;
    }
    else if ((strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "PCMU") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G711") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G.711") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G711U") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G.711U") == 0))
    {
        pstAnjAudioCfg->encodeType = MEDIA_CODEC_AUDIO_G711U;
    }
    else if ((strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "PCM") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G.722") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G722") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G.726") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "G726") == 0))
    {
        pstAnjAudioCfg->encodeType = MEDIA_CODEC_AUDIO_PCM;
    }
    else if ((strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "MPEG4-GENERIC") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "AACG4-GENERIC") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "AAC") == 0) ||
             (strcasecmp(stAudioCfg.audioEncode.audioEncodeType.typeName, "MP4A") == 0))
    {
        encPersize = AUDIO_AAC_PERIOD_SIZE;
        pstAnjAudioCfg->encodeType = MEDIA_CODEC_AUDIO_AAC;
    }
    else
    {
        __ERR("invalid encodeType:%s\n", stAudioCfg.audioEncode.audioEncodeType.typeName);
        return -1;
    }
    pstAnjAudioCfg->algo_aec_enable = (stAudioCfg.audioCapture.aec_enable > 0) ? 1 : 0;

    pstAnjAudioCfg->algo_anr_enable = AUDIO_ANR_ENABLE;
    pstAnjAudioCfg->algo_agc_enable = AUDIO_AGC_ENABLE;
    pstAnjAudioCfg->algo_eq_enable = AUDIO_EQ_ENABLE;
    if (AUDIO_BF_ENABLE)
    {
        pstAnjAudioCfg->algo_bf_enable = (stAudioCfg.audioCapture.channels == 2) ? 1 : 0;
    }
    else
    {
        pstAnjAudioCfg->algo_bf_enable = 0;
    }

    EventResult event_result = {0};
    int config_samplerate = stAudioCfg.audioCapture.samplerate;

    eventhub_publish(EVENTHUB_CLASS_MEDIA, EVENTHUB_WAVE_GET_STATUS, &event_result, NULL);
    pstAnjAudioCfg->wave_active = (event_result.ret == 1) ? 1 : 0;

    pstAnjAudioCfg->samplerate = anj_mw_media_audio_ai_capture_rate(config_samplerate);
    pstAnjAudioCfg->ao_samplerate = config_samplerate;
    pstAnjAudioCfg->bitwidth = stAudioCfg.audioCapture.bitspersample;
    pstAnjAudioCfg->chn = stAudioCfg.audioCapture.channels;
    pstAnjAudioCfg->persize = (pstAnjAudioCfg->samplerate * pstAnjAudioCfg->bitwidth) / 8 / 25;
    if (pstAnjAudioCfg->persize % 128 != 0)
    {
        pstAnjAudioCfg->persize = 128 * (pstAnjAudioCfg->persize / 128);
    }
    pstAnjAudioCfg->aec_enable = stAudioCfg.audioCapture.aec_enable;
    pstAnjAudioCfg->aenc_enable = stAudioCfg.audioEncode.enable;
    pstAnjAudioCfg->ai_volume = stAudioCfg.audioCapture.volume_capture;
    pstAnjAudioCfg->ai_amplify = stAudioCfg.audioCapture.amplify;
    pstAnjAudioCfg->ao_volume = stAudioCfg.audioCapture.volume_play;
    pstAnjAudioCfg->pcm_data_cb = anj_audio_pcm_data_cb;

    if (pstAnjAudioCfg->aenc_enable)
    {
        int encSampleRate = stAudioCfg.audioEncode.sampleRate;

        pstEncHandle = AU_Create(NULL, MEDIA_CODEC_AUDIO_PCM, pstAnjAudioCfg->samplerate, pstAnjAudioCfg->chn,
                                 pstAnjAudioCfg->encodeType, encSampleRate, pstAnjAudioCfg->chn, encPersize, anj_audio_aenc_data_cb);
        if (pstEncHandle != NULL)
        {
            __INFO("AU_Create success.\n");
        }
        else
        {
            __INFO("AU_Create failed.\n");
            return -1;
        }
    }
    return 0;
}

int anj_audio_ao_play_thread(void *ctx, int *bStart)
{
    __LOG_ENTER();
    // AnjAudioConfig *pstAnjAudioCfg = (AnjAudioConfig *)ctx;

    while (bStart && *bStart != 0)
    {
        usleep(200 * 1000);

        int should_play = 0;
        audioplay_info cur_play_info = {0};

        anj_mutex_lock(&s_gAudioPlayFileMutex);

        if (s_stAudioPlayFileManager.has_new_request == 0)
        {
            anj_mutex_unlock(&s_gAudioPlayFileMutex);
            continue;
        }

        should_play = s_stAudioPlayFileManager.has_new_request;
        s_stAudioPlayFileManager.has_new_request = 0;
        s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_PLAYING;
        memcpy(&cur_play_info, &s_stAudioPlayFileManager.current, sizeof(audioplay_info));

        anj_mutex_unlock(&s_gAudioPlayFileMutex);

        if (should_play)
        {
            anj_audio_do_play_file(&cur_play_info);

            anj_mutex_lock(&s_gAudioPlayFileMutex);
            if (s_stAudioPlayFileManager.next.pAudioFile[0] != '\0')
            {
                memcpy(&s_stAudioPlayFileManager.current, &s_stAudioPlayFileManager.next, sizeof(audioplay_info));
                memset(&s_stAudioPlayFileManager.next, 0, sizeof(s_stAudioPlayFileManager.next));
                s_stAudioPlayFileManager.has_new_request = 1;
            }
            else
            {
                s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_READY;
                s_stAudioPlayFileManager.has_new_request = 0;
                memset(&s_stAudioPlayFileManager.current, 0, sizeof(s_stAudioPlayFileManager.current));
            }
            anj_mutex_unlock(&s_gAudioPlayFileMutex);
        }
    }

    __LOG_LEAVE();
    return 0;
}

static void anj_audio_play_file_init()
{
    memset(&s_stAudioPlayFileManager, 0, sizeof(s_stAudioPlayFileManager));

    s_stAudioAoPlayThread.bAutoDestroy = 0;
    strncpy(s_stAudioAoPlayThread.iThreadName, "ao_play_thread", sizeof(s_stAudioAoPlayThread.iThreadName) - 1);
    s_stAudioAoPlayThread.iThreadjob.ctx = &gstAnjAudioCfg;
    s_stAudioAoPlayThread.iThreadjob.func = anj_audio_ao_play_thread;
    anj_thread_task_create(&s_stAudioAoPlayThread);
}

static void anj_audio_play_file_uninit()
{
    anj_thread_task_destroy(&s_stAudioAoPlayThread, 0);

    anj_mutex_lock(&s_gAudioPlayFileMutex);
    s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_READY;
    anj_mutex_unlock(&s_gAudioPlayFileMutex);
}

static int anj_audio_init(void)
{
    if (s_stAudioInit)
    {
        __ERR("had been init!\n");
        return 0;
    }
    anj_audio_attr_init(&gstAnjAudioCfg);
    memset(&s_stAudioAiAoTestInfo, 0, sizeof(s_stAudioAiAoTestInfo));
    int iRet = anj_mw_media_audio_init(&gstAnjAudioCfg);
    s_stAudioInit = 1;

    anj_audio_play_file_init();

    return iRet;
}

static int anj_audio_uninit(void)
{
    if (s_stAudioInit == 0)
    {
        __ERR("not init!\n");
        return 0;
    }
    s_stAudioInit = 0;

    anj_mutex_lock(&s_stAudioPlayInfoMutex);
    if (s_stAudioPlayInfo.playtime > 0)
    {
        anj_mw_hwctrl_switch_ao_close();
    }
    memset(&s_stAudioPlayInfo, 0, sizeof(s_stAudioPlayInfo));
    anj_mutex_unlock(&s_stAudioPlayInfoMutex);

    anj_audio_play_file_uninit();

    int iRet = anj_mw_media_audio_uninit();
    if (pTalkDecHandle)
    {
        AU_Destroy(pTalkDecHandle, s_eTalkDecSrcCodec, MEDIA_CODEC_AUDIO_PCM);
        pTalkDecHandle = NULL;
    }
    s_eTalkDecSrcCodec = MEDIA_CODEC_NONE;
    s_iTalkDecSrcSampleRate = 0;

    if (pstEncHandle)
    {
        AU_Destroy(pstEncHandle, MEDIA_CODEC_AUDIO_PCM, gstAnjAudioCfg.encodeType);
        pstEncHandle = NULL;
    }

    if (s_stAudioAiAoTestInfo.buff)
    {
        anj_mw_free(s_stAudioAiAoTestInfo.buff);
        s_stAudioAiAoTestInfo.buff = NULL;
    }

    return iRet;
}

static int anj_audio_restart_thread(void *ctx, int *bStart)
{
    int iRet = 0;

    (void)ctx;

    while (bStart && *bStart)
    {
        iRet = anj_audio_uninit();
        if (iRet)
        {
            __ERR("anj_audio_uninit failed:%d!\n", iRet);
            return -1;
        }

        usleep(300 * 1000); // 先延时300ms看看

        iRet = anj_audio_init();
        if (iRet)
        {
            __ERR("anj_audio_init failed:%d!\n", iRet);
            return -1;
        }

        break;
    }

    return 0;
}

int anj_audio_play_file(audioplay_info *pstAudioInfo)
{
    int iRet = 0;
    int bPlaying = 0;

    if (pstAudioInfo == NULL || s_stAudioInit == 0)
    {
        return -1;
    }

    if (s_stAudioPlayInfo.bTalk)
    {
        return 0;
    }

    anj_mutex_lock(&s_gAudioPlayFileMutex);

    bPlaying = (s_stAudioPlayFileManager.play_status == AUDIO_FILE_PLAY_PLAYING);

    if (s_stAudioPlayFileManager.current.pAudioFile[0] == '\0')
    {
        memcpy(&s_stAudioPlayFileManager.current, pstAudioInfo, sizeof(audioplay_info));
        s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_READY;
        s_stAudioPlayFileManager.has_new_request = 1;
        __INFO("audio play file:%s priority:%d!\n", pstAudioInfo->pAudioFile, pstAudioInfo->priority);
    }
    else
    {
        switch (pstAudioInfo->playaction)
        {
        case AUDIO_PLAY_ACTION_WAIT_PREV:
            if (bPlaying && s_stAudioPlayFileManager.current.playtimes != 1)
            {
                s_stAudioPlayFileManager.current.playtimes = 1;
            }
            memcpy(&s_stAudioPlayFileManager.next, pstAudioInfo, sizeof(audioplay_info));
            s_stAudioPlayFileManager.has_new_request = 1;
            __INFO("audio play file:%s priority:%d wait prev\n",
                   pstAudioInfo->pAudioFile, pstAudioInfo->priority);
            break;

        case AUDIO_PLAY_ACTION_SKIP_IF_PLAY:
            if (bPlaying)
            {
                if (s_stAudioPlayFileManager.current.playtimes != 1)
                {
                    s_stAudioPlayFileManager.current.playtimes = 1;
                }
                __ERR("audio play file skip when playing!\n");
            }
            else
            {
                memcpy(&s_stAudioPlayFileManager.current, pstAudioInfo, sizeof(audioplay_info));
                s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_READY;
                s_stAudioPlayFileManager.has_new_request = 1;
                __INFO("audio play file:%s priority:%d!\n", pstAudioInfo->pAudioFile, pstAudioInfo->priority);
            }
            break;

        default:
            /* 数字越小优先级越高；正在播的更高或相同则丢弃 */
            if (s_stAudioPlayFileManager.current.priority <= pstAudioInfo->priority)
            {
                __ERR("audio play file:%s priority:%d <= playing priority:%d!\n",
                      pstAudioInfo->pAudioFile, pstAudioInfo->priority,
                      s_stAudioPlayFileManager.current.priority);
                iRet = -1;
            }
            else if (bPlaying)
            {
                if (s_stAudioPlayFileManager.current.playtimes != 1)
                {
                    s_stAudioPlayFileManager.current.playtimes = 1;
                }
                memcpy(&s_stAudioPlayFileManager.next, pstAudioInfo, sizeof(audioplay_info));
                s_stAudioPlayFileManager.has_new_request = 1;
                __INFO("audio play file:%s priority:%d preempt save\n",
                       pstAudioInfo->pAudioFile, pstAudioInfo->priority);
            }
            else
            {
                memcpy(&s_stAudioPlayFileManager.current, pstAudioInfo, sizeof(audioplay_info));
                s_stAudioPlayFileManager.play_status = AUDIO_FILE_PLAY_READY;
                s_stAudioPlayFileManager.has_new_request = 1;
                __INFO("audio play file:%s priority:%d!\n", pstAudioInfo->pAudioFile, pstAudioInfo->priority);
            }
            break;
        }
    }

    anj_mutex_unlock(&s_gAudioPlayFileMutex);
    return iRet;
}

void anj_audio_play_file_stop(void)
{
    if (s_stAudioInit == 0)
        return;
    anj_mutex_lock(&s_gAudioPlayFileMutex);
    if (s_stAudioPlayFileManager.play_status == AUDIO_FILE_PLAY_PLAYING)
    {
        if (s_stAudioPlayFileManager.current.playtimes > 1)
        {
            s_stAudioPlayFileManager.current.playtimes = 1;
            __INFO("audio play file fast set play time to 1!\n");
        }

        memset(&s_stAudioPlayFileManager.next, 0, sizeof(s_stAudioPlayFileManager.next));
    }
    anj_mutex_unlock(&s_gAudioPlayFileMutex);
}

int anj_audio_play_file_status_get(void)
{
    if (s_stAudioInit == 0)
        return 0;
    AudioFilePlayStatus status = AUDIO_FILE_PLAY_READY;
    anj_mutex_lock(&s_gAudioPlayFileMutex);
    status = s_stAudioPlayFileManager.play_status;
    anj_mutex_unlock(&s_gAudioPlayFileMutex);

    return (status == AUDIO_FILE_PLAY_READY) ? 0 : 1;
}

void anj_audio_restart()
{
    if (s_stAudioInit == 0)
        return;
    static anj_thread_s stAudioResThread = {0};

    if (stAudioResThread.start != 0 && stAudioResThread.end == 0)
    {
        return;
    }

    memset(&stAudioResThread, 0, sizeof(anj_thread_s));
    stAudioResThread.bAutoDestroy = 1;
    strncpy(stAudioResThread.iThreadName, "audio_restart", sizeof(stAudioResThread.iThreadName) - 1);
    stAudioResThread.iThreadjob.ctx = (void *)&stAudioResThread;
    stAudioResThread.iThreadjob.func = anj_audio_restart_thread;
    anj_thread_task_create(&stAudioResThread);
}

int anj_audio_ai_volume_set(int volume, int amplify)
{
    if (s_stAudioInit == 0)
        return 0;
    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
        if (volume > 80)
            volume = 80;
    return anj_mw_media_audio_ai_volume_set(volume, amplify);
}

int anj_audio_ao_volume_set(int volume)
{
    if (s_stAudioInit == 0)
        return 0;
    if (ANJ_CUSTOMER_TYPE == CUSTOMER_WTD)
        if (volume > 85)
            volume = 85;
    return anj_mw_media_audio_ao_volume_set(volume);
}

void anj_audio_play_data(char *data, int data_len, int bTalk, media_codec_type_e codec_type, int samplerate)
{
    if (s_stAudioInit == 0)
        return;
    int srcSampleRate = samplerate;

    if (codec_type == MEDIA_CODEC_AUDIO_G711U)
    {
        srcSampleRate = AU_SampleRate_8000HZ;
    }
    else if (codec_type == MEDIA_CODEC_AUDIO_G711A)
    {
        srcSampleRate = AU_SampleRate_8000HZ;
    }
    else if (codec_type == MEDIA_CODEC_AUDIO_PCM)
    {
        if ((bTalk == 0) || (samplerate == AU_SampleRate_8000HZ))
        {
            anj_audio_play_pcm(data, data_len, bTalk);
            return;
        }

        if (srcSampleRate <= 0)
        {
            srcSampleRate = gstAnjAudioCfg.ao_samplerate;
        }
    }
    else if (codec_type == MEDIA_CODEC_AUDIO_MP3)
    {
        if (srcSampleRate <= 0)
        {
            srcSampleRate = AU_SampleRate_16000HZ;
        }
    }
    else if (codec_type == MEDIA_CODEC_AUDIO_AAC)
    {
        if (srcSampleRate <= 0)
        {
            srcSampleRate = AU_SampleRate_16000HZ;
        }
    }
    else
    {
        __ERR("unsupported audio codec_type:%d\n", codec_type);
        return;
    }

    // 配置当前解码的talk状态，解码回调里会用到
    s_iTalkDecBTalk = bTalk;

    // 如果当前解码器的输入格式和本次不一致，销毁重建解码器
    if ((pTalkDecHandle != NULL) &&
        ((s_eTalkDecSrcCodec != codec_type) || (s_iTalkDecSrcSampleRate != srcSampleRate)))
    {
        AU_Destroy(pTalkDecHandle, s_eTalkDecSrcCodec, MEDIA_CODEC_AUDIO_PCM);
        pTalkDecHandle = NULL;
    }

    // 如果解码器不存在，创建解码器
    if (pTalkDecHandle == NULL)
    {
        __INFO("AU_Create talk handle. srcCodec:%d srcSampleRate:%d dstSampleRate:%d chn:%d\n",
               codec_type, srcSampleRate, gstAnjAudioCfg.ao_samplerate, gstAnjAudioCfg.chn);

        pTalkDecHandle = AU_Create(NULL,
                                   codec_type,
                                   (AU_SampleRate_e)srcSampleRate,
                                   gstAnjAudioCfg.chn,
                                   MEDIA_CODEC_AUDIO_PCM,
                                   (AU_SampleRate_e)gstAnjAudioCfg.ao_samplerate,
                                   gstAnjAudioCfg.chn,
                                   AUDIO_PERIOD_SIZE,
                                   anj_audio_adec_data_cb);
        if (pTalkDecHandle == NULL)
        {
            __ERR("AU_Create failed for talk handle, fallback to direct play.\n");
            return;
        }

        s_eTalkDecSrcCodec = codec_type;
        s_iTalkDecSrcSampleRate = srcSampleRate;
    }

    // 解码并播放
    AU_Codec(pTalkDecHandle, codec_type, MEDIA_CODEC_AUDIO_PCM,
             (uint8_t *)data, data_len, 0, 0);
}

void anj_audio_talk_reset(void)
{
    if (pTalkDecHandle)
    {
        AU_Destroy(pTalkDecHandle, s_eTalkDecSrcCodec, MEDIA_CODEC_AUDIO_PCM);
        pTalkDecHandle = NULL;
    }

    s_eTalkDecSrcCodec = MEDIA_CODEC_NONE;
    s_iTalkDecSrcSampleRate = 0;
    s_iTalkDecBTalk = 0;
}

int anj_audio_ai_mute_set(int enable)
{
    if (s_stAudioInit == 0)
        return 0;
    __INFO("Mute enable:%d\n", enable);
    return anj_mw_media_audio_ai_mute_set(enable);
}

int anj_audio_ao_play_file_status_get()
{
    if (s_stAudioInit == 0)
        return 0;
    int play_status = 0;

    anj_mutex_lock(&s_stAudioPlayInfoMutex);
    if (s_stAudioPlayInfo.playtime > 0 && (anj_mw_get_cputime_ms(NULL) - s_stAudioPlayInfo.playtime) < AUDIO_AO_END_TIME)
    {
        play_status = 1;
    }
    anj_mutex_unlock(&s_stAudioPlayInfoMutex);
    return play_status;
}

void anj_audio_aiao_test_start()
{
    if (s_stAudioInit == 0)
        return;
    __INFO("audio aiao test start\n");
    anj_mutex_lock(&s_stAudioAiAoTestMutex);

    if (s_stAudioAiAoTestInfo.buff == NULL)
    {
        s_stAudioAiAoTestInfo.buff = (char *)anj_mw_malloc(AUDIO_AIAO_TEST_BUF_SIZE);
        if (s_stAudioAiAoTestInfo.buff == NULL)
        {
            anj_mutex_unlock(&s_stAudioAiAoTestMutex);
            __ERR("audio test buf malloc failed\n");
            return;
        }
    }

    s_aiao_test_play_scheduled = 0;
    s_stAudioAiAoTestInfo.startup = 1;
    s_stAudioAiAoTestInfo.finish_status = 0;
    s_stAudioAiAoTestInfo.buf_pos = 0;
    s_stAudioAiAoTestInfo.start_pts = 0;
    memset(s_stAudioAiAoTestInfo.buff, 0, AUDIO_AIAO_TEST_BUF_SIZE);

    anj_mutex_unlock(&s_stAudioAiAoTestMutex);
}

void anj_audio_aiao_test_stop()
{
    if (s_stAudioInit == 0)
        return;
    __INFO("audio aiao test stop\n");
    anj_mutex_lock(&s_stAudioAiAoTestMutex);

    s_stAudioAiAoTestInfo.startup = 0;
    s_stAudioAiAoTestInfo.finish_status = 0;
    s_stAudioAiAoTestInfo.buf_pos = 0;
    s_stAudioAiAoTestInfo.start_pts = 0;

    if (s_stAudioAiAoTestInfo.buff != NULL)
    {
        memset(s_stAudioAiAoTestInfo.buff, 0, AUDIO_AIAO_TEST_BUF_SIZE);
    }
    anj_mutex_unlock(&s_stAudioAiAoTestMutex);
}

AudioAiAoTestInfo_t *anj_audio_aiao_test_info_get()
{
    return &s_stAudioAiAoTestInfo;
}

void anj_audio_prompt_play(char *filepath, char *filename, int cnt)
{
    if (s_stAudioInit == 0)
        return;
    if (filename == NULL || strlen(filename) <= 0 || filepath == NULL || strlen(filepath) <= 0 || cnt <= 0)
    {
        __ERR("input invalid!\n");
        return;
    }
    __INFO("filepath:%s filename:%s cnt:%d\n", filepath, filename, cnt);
    SystemConfig *pSystemConfig = getSystemConfig();
    AudioPromptConfig *pAudioPromptConfig = &pSystemConfig->audioPromptCfg;
    DevInfo *pstDevInfo = getDevInfo();

    if (pstDevInfo->activated == 0)
    {
        __ERR("audio play return here because not activated!\n");
        return;
    }

    int bind = anj_mw_file_exists(P2P_DEVICEBIND_FLAG);
    if (bind && (strcmp(filepath, ANJ_MP3_BIND_PATH) == 0))
    {
        return;
    }

    if (pAudioPromptConfig->network == 0 && (strcmp(filepath, ANJ_MP3_NETWORK_PATH) == 0))
    {
        return;
    }
    if (pAudioPromptConfig->startup == 0 && strcmp(filepath, ANJ_MP3_DEVICE_PATH) == 0)
    {
        return;
    }
    if (pAudioPromptConfig->ota == 0 && strcmp(filepath, ANJ_MP3_OTA_PATH) == 0)
    {
        return;
    }
    if (pAudioPromptConfig->sdcard == 0 && strcmp(filepath, ANJ_MP3_SDCARD_PATH) == 0)
    {
        return;
    }

    sleep(1);
    audioplay_info audio_info = {0};

    if (strcmp(filename, UPLOAD_MP3_FILE_NAME) == 0 ||
        strcmp(filename, ANJ_MP3_DI_DI) == 0)
    {
        snprintf(audio_info.pAudioFile, sizeof(audio_info.pAudioFile), "%s/%s", filepath, filename);
    }
    else
    {
        if (!strcmp(pSystemConfig->miscCfg.language, "en-us") || !strcmp(pSystemConfig->miscCfg.language, "en_us"))
        {
            snprintf(audio_info.pAudioFile, sizeof(audio_info.pAudioFile), "%s/en/%s", filepath, filename);
        }
        else
        {
            snprintf(audio_info.pAudioFile, sizeof(audio_info.pAudioFile), "%s/ch/%s", filepath, filename);
        }
    }

    audio_info.playtimes = cnt;
    audio_info.playaction = AUDIO_PLAY_ACTION_WAIT_PREV;
    audio_info.priority = AUDIO_PLAY_PRIORITY_MAX;
    audio_info.encodeType = MEDIA_CODEC_AUDIO_MP3;
    anj_audio_play_file(&audio_info);
}

int anj_audio_mp3_file_list_query(file_query_result *pstFileQueryResult, int skip_count, int page_size)
{
    if (s_stAudioInit == 0)
        return 0;
    int file_count = 0;
    char szPath[64] = {0};
    if (skip_count == 0)
    {
        snprintf(szPath, sizeof(szPath), "%s/alarm", ANJ_MP3_DEFAULT_PATH);
        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".mp3");

        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".wav");

        memset(szPath, 0, sizeof(szPath));
        SystemConfig *pSystemCfg = (SystemConfig *)getSystemConfig();
        if (strlen(pSystemCfg->miscCfg.language) == 0 || 0 == strcasecmp(pSystemCfg->miscCfg.language, "zh_cn") || 0 == strcasecmp(pSystemCfg->miscCfg.language, "zh_tw"))
        {
            snprintf(szPath, sizeof(szPath), "%s/alarm/ch", ANJ_MP3_DEFAULT_PATH);
        }
        else
        {
            snprintf(szPath, sizeof(szPath), "%s/alarm/en", ANJ_MP3_DEFAULT_PATH);
        }

        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".mp3");

        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".wav");

        memset(szPath, 0, sizeof(szPath));
        snprintf(szPath, sizeof(szPath), "%s", DATA_BLOCK_MOUNT_PATH);
        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".mp3");
    }

    memset(szPath, 0, sizeof(szPath));
    snprintf(szPath, sizeof(szPath), "%s/mp3", OEM_MOUNT_PATH);
    file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".mp3");

    if (SUPPORT_NAND_FLASH == 0)
    {
        memset(szPath, 0, sizeof(szPath));
        snprintf(szPath, sizeof(szPath), "/data/mp3");
        file_count += query_normal_file_in_dir(pstFileQueryResult, skip_count, page_size, szPath, ".mp3");
    }

    __INFO("query mp3 file count:%d\n", file_count);
    return file_count;
}

REGISTER_MODULE(anj_audio, MODULE_PRIORITY_AUDIO);
