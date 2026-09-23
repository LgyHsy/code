#ifndef __UNV_DEF_H__
#define __UNV_DEF_H__

#define HTTP_PUT_OK 0

#define UNV_SMART_ATTCLL    "Smart/AttributeCollect"
#define UNV_SMART_WKSTAT    "Smart/WorkingStatus"
#define UNV_SMART_MUXR      "Smart/MutexRelationInfos"
#define UNV_SMART_CAP       "Smart/Capabilities"

#define UNV_MEDIA_AUDIO_CAP     "Media/Audio/Capabilities"
#define UNV_MEDIA_AUDIO_INPUT   "Media/Audio/Input"
#define UNV_MEDIA_AUDIO_OUTPUT  "Media/Audio/Output"

#define UNV_IMAGE_CAPABILITIES  "Image/Capabilities"
#define UNV_IMAGE_ENHANCE       "Image/Enhance"
#define UNV_IMAGE_LAMPCTRL      "Image/LampCtrl"

#define UNV_SYSTEM_CAP          "System/Capabilities"
#define UNV_SYSTEM_AUDIOFILE    "System/AudioFile/Info"
#define UNV_SYSTEM_PHOTO        "System/PhotoServer"
#define UNV_SUBCRIPTION         "Subscription"
#define UNV_SUBCRIPTION_X       "Subscription/"

#define UNV_ALARM_CAPPABILITY       "Alarm/Capabilities"

#define UNV_ALARM_HUMAN_RULE        "Alarm/HumanShapeDetection/Rule"
#define UNV_ALARM_HUMAN_AREAS       "Alarm/HumanShapeDetection/Areas"
#define UNV_ALARM_HUMAN_WEEKPLAN    "Alarm/HumanShapeDetection/WeekPlan"

#define UNV_ALARM_MOTION_RULE       "Alarm/MotionDetection/Rule"
#define UNV_ALARM_MOTION_AREAS      "Alarm/MotionDetection/Areas/RECTAreas"
#define UNV_ALARM_MOTION_AREAS0     "Alarm/MotionDetection/Areas/RECTAreas/0"
#define UNV_ALARM_MOTION_GRIDAREA   "Alarm/MotionDetection/Areas/GridArea"
#define UNV_ALARM_MOTION_LINK       "Alarm/MotionDetection/LinkageActions"
#define UNV_ALARM_MOTION_WEEKPLAN   "Alarm/MotionDetection/WeekPlan"

#define UNV_SMART_VEHICLE_WEEKPLAN          "Smart/VehicleDetection/WeekPlan"
#define UNV_SMART_VEHICLE_DETECT_RULE       "Smart/VehicleDetection/Rule"
#define UNV_SMART_VEHICLE_DETECT_AREAS      "Smart/VehicleDetection/Areas"
#define UNV_SMART_VEHICLE_DETECT_AREAS0     "Smart/VehicleDetection/Areas/0"

#define UNV_SMART_CROSSLINE_RULE        "Smart/CrossLineDetection/Rule"	//拌线越界
#define UNV_SMART_CROSSLINE_AREAS       "Smart/CrossLineDetection/Areas"	//拌线越界
#define UNV_SMART_CROSSLINE_AREAS0      "Smart/CrossLineDetection/Areas/0"
#define UNV_SMART_CROSSLINE_AREAS1      "Smart/CrossLineDetection/Areas/1"
#define UNV_SMART_CROSSLINE_AREAS2      "Smart/CrossLineDetection/Areas/2"
#define UNV_SMART_CROSSLINE_AREAS3      "Smart/CrossLineDetection/Areas/3"
#define UNV_SMART_CROSSLINE_LINK        "Smart/CrossLineDetection/LinkageActions"
#define UNV_SMART_CROSSLINE_WEEKPLAN    "Smart/CrossLineDetection/WeekPlan"

#define UNV_SMART_ACCESSZONE_RULE       "Smart/AccessZone/Rule"
#define UNV_SMART_ACCESSZONE_AREAS      "Smart/AccessZone/Areas"
#define UNV_SMART_ACCESSZONE_AREAS0     "Smart/AccessZone/Areas/0"
#define UNV_SMART_ACCESSZONE_AREAS1     "Smart/AccessZone/Areas/1"
#define UNV_SMART_ACCESSZONE_AREAS2     "Smart/AccessZone/Areas/2"
#define UNV_SMART_ACCESSZONE_AREAS3     "Smart/AccessZone/Areas/3"
#define UNV_SMART_ACCESSZONE_LINK       "Smart/AccessZone/LinkageActions"
#define UNV_SMART_ACCESSZONE_WEEKPLAN   "Smart/AccessZone/WeekPlan"

#define UNV_SMART_LEAVEZONE_RULE        "Smart/LeaveZone/Rule"
#define UNV_SMART_LEAVEZONE_AREAS       "Smart/LeaveZone/Areas"
#define UNV_SMART_LEAVEZONE_AREAS0      "Smart/LeaveZone/Areas/0"
#define UNV_SMART_LEAVEZONE_AREAS1      "Smart/LeaveZone/Areas/1"
#define UNV_SMART_LEAVEZONE_AREAS2      "Smart/LeaveZone/Areas/2"
#define UNV_SMART_LEAVEZONE_AREAS3      "Smart/LeaveZone/Areas/3"
#define UNV_SMART_LEAVEZONE_LINK        "Smart/LeaveZone/LinkageActions"
#define UNV_SMART_LEAVEZONE_WEEKPLAN    "Smart/LeaveZone/WeekPlan"

#define UNV_SMART_INTRU_RULE        "Smart/IntrusionDetection/Rule"
#define UNV_SMART_INTRU_AREAS       "Smart/IntrusionDetection/Areas"
#define UNV_SMART_INTRU_AREAS0      "Smart/IntrusionDetection/Areas/0"
#define UNV_SMART_INTRU_AREAS1      "Smart/IntrusionDetection/Areas/1"
#define UNV_SMART_INTRU_AREAS2      "Smart/IntrusionDetection/Areas/2"
#define UNV_SMART_INTRU_AREAS3      "Smart/IntrusionDetection/Areas/3"
#define UNV_SMART_INTRU_LINK        "Smart/IntrusionDetection/LinkageActions"
#define UNV_SMART_INTRU_WEEKPLAN    "Smart/IntrusionDetection/WeekPlan"

#define UNV_SMART_FACE_ENABLE 	    "Smart/FaceEnable"
#define UNV_SMART_FACE_RULE 		"Smart/FaceDetection/Rule"
#define UNV_SMART_FACE_AREAS 		"Smart/FaceDetection/Areas/Detections"
#define UNV_SMART_FACE_WEEKPLAN 	"Smart/FaceDetection/WeekPlans"
#define UNV_SMART_FACE_LINK 		"Smart/FaceDetection/LinkageActions"

#define UNV_SMART_OBJTARCK_RULE     "Smart/ObjTrack/Rule"


#define UNV_LAPI_SYSTEM_EVENT_SUB   "/LAPI/V1.0/System/Event/Subscription"
#define UNV_LAPI_CHANNELS           "/LAPI/V1.0/Channels"
#define UNV_LAPI_SYSTEM             "/LAPI/V1.0/System"


// 宇视智能业务ID
#define UNV_SMART_ID_FACE_DETECT    0
#define UNV_SMART_ID_AUTO_TRACK     3
#define UNV_SMART_ID_FIX_DETECT     6
#define UNV_SMART_ID_CROSS_LINE     100
#define UNV_SMART_ID_INTRUSION      101
#define UNV_SMART_ID_ACCESS_ZONE    102
#define UNV_SMART_ID_LEAVE_ZONE     103
#define UNV_SMART_ID_SMART_MOTION   107


// 宇视联动动作ID
#define UNV_ACT_ID_AUDIO_ALARM      24      // 声音报警
#define UNV_ACT_ID_LIGHT_ALARM      25      // 灯光报警
#define UNV_ACT_ID_AUDIO_PLAN       28      // 声音报警布防
#define UNV_ACT_ID_LIGHT_PLAN       29      // 灯光报警布防

#endif
