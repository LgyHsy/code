#ifndef _UNVPD_HANDLE_H
#define _UNVPD_HANDLE_H

#include "cJSON.h"


int unv_motion_detect_mode_get();

/******************** image ***********************/

int unv_image_capability_get(cJSON **p);
int unv_image_lampctrl_get(cJSON **p);
int unv_image_enhance_get(cJSON **p);


/******************** media ***********************/

int unv_media_audio_input_get(cJSON **p);
int unv_media_audio_output_get(cJSON **p);
int unv_media_audio_capability_get(cJSON **p);


/******************** system ***********************/
int unv_system_capability_get(cJSON **p);
int unv_system_audiofile_get(cJSON **p);
int unv_system_photoserver_get(cJSON **p);


/******************** plan ***********************/
int unv_get_weekplan_info(cJSON **p);

/******************** smart ***********************/
int unv_smart_working_status_info_get(cJSON **p);
int unv_smart_mutex_relation_info_get(cJSON **p);

int unv_smart_capability_ex_get(cJSON **p);
int unv_smart_capability_get(cJSON **p);

int unv_alarm_capability_get(cJSON **p);

int unv_alarm_smart_motion_week_plan_get(cJSON **p);
int unv_alarm_smart_motion_rule_get(cJSON **p);
int unv_alarm_smart_motion_areas_get(cJSON **p);
int unv_alarm_smart_motion_areas0_get(cJSON **p);
int unv_alarm_smart_motion_linkage_action_get(cJSON **p);

int unv_smart_crossline_one_area_get(cJSON **p, int index);
int unv_smart_crossline_areas_get(cJSON **p);
int unv_smart_crossline_rule_get(cJSON **p);
int unv_smart_crossline_linkage_action_get(cJSON **p);
int unv_smart_crossline_week_plan_get(cJSON **p);

int unv_smart_facedetect_enable_get(cJSON **p);
int unv_smart_facedetect_rule_get(cJSON **p);
int unv_smart_facedetect_areas_get(cJSON **p);
int unv_smart_facedetect_linkage_action_get(cJSON **p);
int unv_smart_facedetect_week_plan_get(cJSON **p);

int unv_smart_autotrack_rule_get(cJSON **p);

int unv_smart_intrusion_one_area_get(cJSON **p, int area_index, int index);
int unv_smart_intrusion_areas_get(cJSON **p, int area_index);
int unv_smart_intrusion_rule_get(cJSON **p, int flag);
int unv_smart_intrusion_linkage_action_get(cJSON **p);
int unv_smart_intrusion_week_plan_get(cJSON **p);

int unv_alarm_human_week_plan_get(cJSON **p);
int unv_alarm_human_rule_get(cJSON **p);
int unv_alarm_human_areas_get(char *pChn, cJSON **p);

int unv_smart_vehicle_rule_get(cJSON **p);
int unv_smart_vehicle_areas0_get(cJSON **p);
int unv_smart_vehicle_areas_get(cJSON **p);


#endif
