#ifndef __ANJ_SERVICE_H__
#define __ANJ_SERVICE_H__

#include "ixml.h"

#define XML_ROOT_NAME1 "XML_TOPSEE"
#define XML_ROOT_NAME2 "XML_ANJVISION"
#define XML_ROOT_NAME3 "XML_WTD"

typedef enum
{
    Storage_NONE = 0,           // 0 - 未插卡
    Storage_OK = 1,             // 1 - 正常
    Storage_UNFORMATED = 2,     // 2 - 未格式化
    Storage_FORMATING = 3,      // 3 - 正在格式化
    Storage_FORMATED = 80,      // 80 - 格式化完成
    Storage_UNINITED = 81,      // 81 - 未初始化
    Storage_FULL = 82,          // 82 - 卡满
    Storage_EXEPTION = 83,      // 83 - 卡异常
    Storage_Mounting = 84,      // 84 - 未挂载/挂载中，用于动态插卡时有10秒等待才挂载，这时不能报未初始化
    Storage_NeedPartition = 85, // 未分区导致的未挂载
} StorageStatus;

typedef enum MSG_CODE_EVENT
{
    EVENT_NOTHING = 0,
    EVENT_UPLOAD,
    EVENT_DOWNLOAD,
    EVENT_TALKBACK,
    EVENT_BUTT,
} MSG_CODE_EVENT;

typedef enum LIGHT_CTRL_INDEX
{
    LIGHT_INDEX_NONE = 0,
    LIGHT_INDEX_WLED = 1,     // 白光灯
    LIGHT_INDEX_RLED = 2,     // 红外灯
    LIGHT_INDEX_RB_ALARM = 3, // 红蓝报警灯
} LIGHT_CTRL_INDEX;

enum
{
	LOG_ALARM_MD, 			   //移动侦测
	LOG_ALARM_PD,   		   //人型侦测
	LOG_ALARM_CAR,   		   //车辆识别
	LOG_ALARM_MOTO,			   //摩托
	LOG_ALARM_ELECTRICBICYCLE, //电单车
	LOG_ALARM_BICYCLE,		   //自行车
	LOG_ALARM_LPR,				//车牌
	LOG_ALARM_GATE,				//越界
	LOG_ALARM_FIRE,				//火焰
	LOG_ALARM_FD,			   	//人脸
	LOG_ALARM_IO,				//IO
	LOG_ALARM_COVER,			//区域遮挡
	LOG_ALARM_ALL,
};

//查询结果信息
typedef struct 
{
	time_t start_time; 		//告警开始时间
	time_t end_time; 		//告警结束时间
	unsigned channel;	    //通道
	unsigned type;		   	//告警类型
	unsigned duration;		//周期
}result_node_t,*preslut_node_t; 

#define VS_RECORD_DISTRIBUTE_LEN 1440 // 一天24小时 * 60分钟

#define VS_REC_TYPE_UNCONDITIONAL 'A' // unconditional recording
#define VS_REC_TYPE_DRIVEN 'B'        // event/alarm-driven recording
#define VS_REC_TYPE_NO_RECORDING 'C'  // no recording
/*nvr已实现类别。IPC需要重新设计分布记录才支持*/
#define VS_REC_TYPE_MD 'D'    // 移动侦测 recording
#define VS_REC_TYPE_IO 'E'    // IO
#define VS_REC_TYPE_PD 'F'    // 人形识别 recording
#define VS_REC_TYPE_LPR 'G'   // 车牌识别 recording
#define VS_REC_TYPE_FD 'H'    // 人脸识别 recording
#define VS_REC_TYPE_COVER 'I' // 视频遮挡 recording
#define VS_REC_TYPE_CAR 'J'
#define VS_REC_TYPE_MOTO 'K'
#define VS_REC_TYPE_ELECTRICBICYCLE 'L'
#define VS_REC_TYPE_BICYCLE 'M'
#define VS_REC_TYPE_VIDEO_GATE 'N'
#define VS_REC_TYPE_Flame 'O'
#define VS_REC_TYPE_REGION_DETECT_ENTER 'P'
#define VS_REC_TYPE_REGION_DETECT_LEAVE 'Q'
#define VS_REC_TYPE_REGION_DETECT_STAY 'R'
#define VS_REC_TYPE_FALLINGOBJECT 'S'

#define UPLOAD_MP3_TO_CFG_MTD_MAX_FILE_SIZE (40 * 1024)
#define UPLOAD_MP3_TO_CFG_MTD_MIN_LEFT_SIZE (50 * 1024)
#define UPLOAD_MP3_FILE_NAME "upload.mp3"
#define UPLOAD_CERTIFICATION_FILE_NAME "https.crt"
#define UPLOAD_KEY_FILE_NAME "https.key"

#define SERVER_NO_FILE_NAME "/mnt/nand/serverNo"

#define USER_DATA_BIN_PATH DATA_BLOCK_MOUNT_PATH "/userdata.bin"
#define BIND_APP_TYPE_FILE "/mnt/nand/flag.app.type"

#define CMD_UNSUPPORT_RESPONSE -2

int anj_service_task_system_get(int cmd, void *data, int *len, int channel);
int anj_service_task_system_set(int cmd, char *data, int channel, int MsgSrc);
char *anj_service_task_media(char *cmdbuf, int cmdlen, IXML_Document *pDoc,
                             char *MsgCode, char *MsgRoot, int MsgSrc);
int anj_service_task_sysctl(IXML_Document *pDoc, char *cmdbuf, int cmdlen, char *MsgType, char *MsgCode,
                            int MsgSrc, int lognum, int channel, char **data);

int anj_service_cmd_convert_xml(char *cmdbuf, char *root_name);
int anj_service_cmd_parse_xml(IXML_Document *pDoc, char *MsgRoot, char *MsgType, char *MsgCode, char *MsgFlag, int *channel);

int anj_service_alarm_event_notify(void *alarm_event);
int anj_service_audio_enc_change();

#endif
