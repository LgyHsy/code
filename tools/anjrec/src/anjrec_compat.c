#include <stddef.h>
#include <string.h>
#include <time.h>

#include <stdarg.h>
#include <stdio.h>

#include "ixml.h"
#include "anj_mw_comm.h"
#include "anj_mw_mem.h"
#include "anj_config_media.h"
#include "anj_config_record.h"
#include "anj_config_gb28181.h"
#include "anj_config_gat1400.h"
#include "anj_config_media.h"
#include "media_util.h"
#include "audio_receiver.h"
#include "anj_audio.h"
#include "anj_config_media.h"
#include "anj_mw_media_isp.h"
#include "anj_mw_media_video.h"
#include "anj_osd.h"
#include "anj_record.h"
#include "anj_sdcard.h"
#include "anj_search.h"
#include "anj_service.h"
#include "anj_service_provider.h"
#include "anj_config_alarm.h"
#include "alarm_link.h"
#include "anj_factory.h"
#include "anj_smart.h"
#include "anj_video.h"
#include "anj_ispctl.h"
#include "media_util.h"
#include "record_log.h"
#include "anj_ser_stream.h"

void anj_audio_prompt_play(char *filepath, char *filename, int cnt)
{
    (void)filepath;
    (void)filename;
    (void)cnt;
}

void anj_audio_restart(void) {}
int anj_audio_ao_volume_set(int volume) { (void)volume; return 0; }
int anj_audio_ai_volume_set(int volume, int amplify) { (void)volume; (void)amplify; return 0; }

int anj_ispctl_update_base_param(VideoCaptureCfg *pNewIspInfo, int cameraIndex)
{
    (void)pNewIspInfo; (void)cameraIndex;
    return 0;
}

void anj_ispctl_config_set(void) {}

int anj_factory_image_flip_cfg_load(int *flip)
{
    if (flip)
    {
        *flip = 0;
    }
    return 0;
}

int anj_osd_cover_set(void) { return 0; }

anj_sdcard_status_e anj_sdcard_status_get(void) { return ANJ_SDCARD_STATUS_NOT_INSERT; }

int audio_talk_status_get(void) { return 0; }

int audio_talk_start(const audio_talk_start_param_t *param)
{
    (void)param;
    return 0;
}
void audio_talk_stop(void) {}

int anj_video_restart(void) { return 0; }
int anj_video_set_config(void *cfg) { (void)cfg; return 0; }
void anj_video_request_idr(int Chn, int VencId) { (void)Chn; (void)VencId; }

void anj_osd_update_config(void) {}
void anj_osd_cloud_set(osd_custom_content_s *pstOsdCustom) { (void)pstOsdCustom; }
int anj_osd_lens_cover_get(void) { return 0; }

int anj_record_uninit(void) { return 0; }
void anj_record_restart(void) {}
int anj_record_lastest_time(struct tm *tm_rec) { (void)tm_rec; return -1; }
int anj_record_pb_is_valid(REC_HANDLE hPoperHandle) { (void)hPoperHandle; return 0; }
REC_HANDLE anj_record_pb_create(int iRecChannel, unsigned int tStartTime, unsigned int tEndTime,
                                unsigned int iEvenType, int iPopId, rec_pb_cb pbCb)
{
    (void)iRecChannel; (void)tStartTime; (void)tEndTime; (void)iEvenType; (void)iPopId; (void)pbCb;
    return NULL;
}
int anj_record_pb_seek(REC_HANDLE pHandle, unsigned int tSeekTime)
{
    (void)pHandle; (void)tSeekTime;
    return -1;
}
int anj_record_pb_release(REC_HANDLE pHandle) { (void)pHandle; return 0; }
int anj_record_pb_pause_set(REC_HANDLE pHandle, int bPause) { (void)pHandle; (void)bPause; return 0; }
int anj_record_pb_speed_set(REC_HANDLE pHandle, int iSpeed) { (void)pHandle; (void)iSpeed; return 0; }

anj_sdcard_info *anj_sdcard_info_get(void)
{
    static anj_sdcard_info info;
    memset(&info, 0, sizeof(info));
    info.eStatus = ANJ_SDCARD_STATUS_NOT_INSERT;
    return &info;
}

int anj_sdcard_umount(void) { return 0; }
int anj_sdcard_mount_index_get(void) { return 0; }

void audio_talk_status_set(int status) { (void)status; }

int anj_factory_defcfg_load(FactoryDefaultCfg *pDefaultCfg) { (void)pDefaultCfg; return -1; }

int anj_factory_init(void) { return 0; }
int anj_factory_defcfg_get(const char *xmlBuf, FactoryDefaultCfg *pDefaultCfg)
{
    (void)xmlBuf; (void)pDefaultCfg;
    return -1;
}
int anj_factory_defcfg_save(FactoryDefaultCfg *pDefaultCfg) { (void)pDefaultCfg; return -1; }
int anj_factory_test(factory_test_mode_e mode) { (void)mode; return -1; }
int anj_factory_wifi_connect_set(char *ssid, char *passwd)
{
    (void)ssid; (void)passwd;
    return -1;
}

smart_mask_e anj_smart_mask_get(void) { return SMART_NULL_MASK; }
void anj_smart_restart(void) {}

int anj_onvif_restart(void) { return 0; }

void anj_osd_polygon_update(Polygon *pstPolygon) { (void)pstPolygon; }
void anj_osd_cross_line_update(VideoGateAlarm *pstVideoGate) { (void)pstVideoGate; }

int anj_config_alarm_get(IXML_Node *pNode, AlarmConfig *pAlarmCfg, int camera_index, int bMsg)
{
    (void)pNode; (void)pAlarmCfg; (void)camera_index; (void)bMsg;
    return 0;
}

char *anj_config_alarm_conver_xml(AlarmConfig *pAlarmCfg)
{
    (void)pAlarmCfg;
    char *p = (char *)anj_mw_malloc(32);
    if (p)
    {
        strcpy(p, "<AlarmConfig>\r\n</AlarmConfig>");
    }
    return p;
}

int anj_service_provider_init_all(void) { return 0; }
int anj_service_provider_uninit_all(void) { return 0; }
int anj_service_provider_alarm_event_notify(void *event) { (void)event; return 0; }
int anj_service_provider_audio_enc_change(void) { return 0; }

int anj_alarm_event_handle(int chn, AjAlarmCode code, int flag, int level, int newalarm,
                           const char *data, const char *snapfile)
{
    (void)chn;
    (void)code;
    (void)flag;
    (void)level;
    (void)newalarm;
    (void)data;
    (void)snapfile;
    return 0;
}

int anj_mw_media_isp_flip_set(int cameraIndex, int hflip, int vflip)
{
    (void)cameraIndex; (void)hflip; (void)vflip;
    return 0;
}

int audio_talk_feed_audio(char *data, int len, media_codec_type_e type, int samplerate, int bitrate)
{
    (void)data;
    (void)len;
    (void)type;
    (void)samplerate;
    (void)bitrate;
    return 0;
}

int anj_ptz_config_save(void *cfg) { (void)cfg; return 0; }

int anj_mbuf_poper_create(int a, int b, void **handle, void *cb, int c, int d)
{
    (void)a; (void)b; (void)handle; (void)cb; (void)c; (void)d;
    return 0;
}

int anj_mbuf_poper_destroy(void *handle) { (void)handle; return 0; }

int record_log_init(void) { return 0; }
int record_log_uninit(void) { return 0; }
int record_log_flush_file(int bBlockTimes) { (void)bBlockTimes; return 0; }
int record_log_del_file(void) { return 0; }
int record_log_get_file(char *szLogFile, int nLen) { (void)szLogFile; (void)nLen; return -1; }

void record_log_print(unsigned int nLogLevel, const char *psFileName, const char *psFuncName, int line, const char *format, ...)
{
    (void)nLogLevel;
    (void)psFileName;
    (void)psFuncName;
    (void)line;
    char buf[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    __ERR("%s", buf);
}

int anj_config_media_default(MediaConfig *pMediaCfg) { (void)pMediaCfg; return 0; }
int anj_config_media_get(IXML_Node *pNode, MediaConfig *pMediaCfg)
{
    (void)pNode; (void)pMediaCfg;
    return 0;
}
int anj_config_media_save(MediaConfig *pMediaCfg) { (void)pMediaCfg; return 0; }

char *anj_config_media_conver_xml(MediaConfig *pMediaCfg, int camera_index, int bMsg)
{
    (void)pMediaCfg; (void)camera_index; (void)bMsg;
    char *p = (char *)anj_mw_malloc(64);
    if (p)
        strcpy(p, "<MediaConfig>\r\n</MediaConfig>");
    return p;
}

char *anj_config_record_conver_xml(RecordConfig *pRecordCfgArray, int camera_index, int bMsg)
{
    (void)pRecordCfgArray; (void)camera_index; (void)bMsg;
    char *p = (char *)anj_mw_malloc(64);
    if (p)
        strcpy(p, "<RecordConfig>\r\n</RecordConfig>");
    return p;
}

int anj_config_record_get(IXML_Node *pNode, RecordConfig *pRecordCfgArray)
{
    (void)pNode; (void)pRecordCfgArray;
    return 0;
}

int anj_config_gb28181_get(IXML_Node *pNode, GB28181Config *pGb28181Cfg)
{
    (void)pNode; (void)pGb28181Cfg;
    return 0;
}

int anj_config_gat1400_get(IXML_Node *pNode, GAT1400Config *pGat1400Cfg)
{
    (void)pNode; (void)pGat1400Cfg;
    return 0;
}

int anj_config_audio_param_get(media_codec_type_e *audio_type, int *samplerate, int *bitspersample, int *channels)
{
    if (audio_type)
        *audio_type = MEDIA_CODEC_AUDIO_G711A;
    if (samplerate)
        *samplerate = 8000;
    if (bitspersample)
        *bitspersample = 16;
    if (channels)
        *channels = 1;
    return 0;
}

void anj_ser_stream_create(void)
{
}

void anj_ser_stream_release(void)
{
}

/* MMA stubs: anj_sysmng_app_update retains App MMA path; recovery never uses nPhyAddr. */
char *anj_sys_mmap(unsigned long long nPhyAddr, unsigned int len)
{
    (void)nPhyAddr;
    (void)len;
    return NULL;
}

void anj_sys_munmap(void *pVirtualAddress, unsigned int mapsize)
{
    (void)pVirtualAddress;
    (void)mapsize;
}

int anj_sys_alloc(unsigned int u32BlkSize, unsigned long long *phyAddr)
{
    (void)u32BlkSize;
    if (phyAddr)
    {
        *phyAddr = 0;
    }
    return -1;
}

void anj_sys_free(unsigned long long phyAddr)
{
    (void)phyAddr;
}
