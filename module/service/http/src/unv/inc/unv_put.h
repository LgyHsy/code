#ifndef __HTTP_PUT_H__
#define __HTTP_PUT_H__

int ToBin(int a, char *buf);
int ToInt(const char *pbin);


int unv_system_photoserver_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_image_enhance_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_image_lampctrl_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_media_audio_output_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_media_audio_input_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);


int unv_smart_work_status_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_attribute_collect_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_smart_facedetect_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_facedetect_linkage_actions_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_facedetect_enable_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_facedetect_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_facedetect_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_smart_intrusion_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int flag);
int unv_smart_intrusion_linkage_actions_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_intrusion_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int flag);
int unv_smart_intrusion_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_smart_crossline_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_crossline_linkage_action_bright_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_crossline_linkage_action_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_crossline_areas0_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse, int i_flag);
int unv_smart_crossline_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_crossline_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);


int unv_smart_vehicle_areas0_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_vehicle_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_smart_vehicle_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_alarm_motion_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_alarm_motion_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_alarm_motion_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_alarm_motion_linkage_action_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_alarm_human_week_plan_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_alarm_human_areas_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_alarm_human_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

int unv_smart_autotrack_rule_set(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);


int unv_subcription_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_subcription_refresh_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);
int unv_subcription_delete_handle(void* pInst, const char* http_url, const char *pMsgBody, cb_func_http_response pCbResponse);

#endif
