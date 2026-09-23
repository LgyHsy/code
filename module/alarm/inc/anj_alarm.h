#ifndef __ANJ_ALARM_H__
#define __ANJ_ALARM_H__

#ifdef __cplusplus
extern "C"
{
#endif

int anj_alarm_ai_detect_start(int camera, int class_id);

int anj_alarm_audio_lsa_start();

int anj_alarm_audio_cry_start();

int anj_alarm_sd0_spacelow_start();

int anj_alarm_gpio3_high2low_start(int chn);

int anj_alarm_gpio3_low2high_start(int chn);

int anj_alarm_video_cover_start();

int anj_alarm_video_lost_start();

int anj_alarm_video_gate_start();

int anj_alarm_motion_detect_start(int camera);

int anj_alarm_ioinput_manual_trigger();

int anj_alarm_ioout_channel_apply(int portIndex, const char *triggerType);

int anj_alarm_sdcard_format_start(int bSuccess);

#ifdef __cplusplus
}
#endif

#endif
