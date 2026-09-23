#ifndef _ANJ_CONFIG_H_
#define _ANJ_CONFIG_H_

#include <pthread.h>

#include "ixml.h"
#include "anj_comm.h"
#include "anj_config_media.h"
#include "anj_config_stream.h"
#include "anj_config_record.h"
#include "anj_config_network.h"
#include "anj_config_gb28181.h"
#include "anj_config_gat1400.h"
#include "anj_config_alarm.h"
#include "anj_config_system.h"
#include "anj_config_oem.h"
#include "anj_config_platform.h"
#include "anj_config_server.h"
#include "anj_config_version.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CONFIG_FILE_PATH DATA_BLOCK_MOUNT_PATH "/config.xml"
#define CONFIG_FILE_DEFAULT_PATH "/opt/ch/config.default.%s.xml"
#define CONFIG_PTZ_PATH DATA_BLOCK_MOUNT_PATH "/ptz_config.xml"

#define AJ_CUST_PATH_NAME "cust"
#define AJ_XML_ONVIF_OEM_FILE_NAME "onvif_oem.xml"

#define DZOOM_CUST_SETTING_FILE "/dzoom.config.xml"                            // 客户定制的数字变倍倍率设置
#define DZOOM_CUST_SETTING_FILE_FULL DATA_BLOCK_MOUNT_PATH "/dzoom.config.xml" // 客户定制的数字变倍倍率设置

#define AJ_XML_CUR_CONFIG_FILE_NAME "currentconfig.xml"
#define FAKE_VIDEO_RES_FILE "fakevideo.xml"

typedef struct
{
    SystemConfig systemCfg;
    ServerConfig serverCfg;
    MediaStreamConfig mediaStreamCfg;
    PlatformConfig platformCfg;
    RecordConfig recordCfg[ANJ_CAMERA_MAX_NUMS];
    NetworkConfigNew networkCfgNew;
    GB28181Config gb28181Cfg;
    GAT1400Config gat1400Cfg;
    ConfigVersion versionCfg;
    MediaConfig mediaCfg;
    AlarmConfig alarmCfg;
    pthread_rwlock_t rwlock;
} GlobalConfig;

char *anj_config_devinfo_conver_xml();

char *anj_config_daytime_conver_xml(WorkDayTime *pWorkDayTime);

char *anj_config_timespan_conver_xml(TimeSpanCfg *pTimeSpan);

char *anj_config_timespan_list_conver_xml(TimeSpanList *pTimeSpanList);

int anj_config_timespan_get(IXML_Node *pNode, TimeSpanCfg *pTimeSpan);

int anj_config_timespan_list_get(IXML_Node *pNode, TimeSpanList *pTimeSpanList);

int anj_config_daytimespan_get(IXML_Node *pNode, DayTimeSpan *pDayTimeSpan);

int anj_config_daytime_get(IXML_Node *pNode, WorkDayTime *pWorkDayTime);

char *anj_config_polygon_conver_xml(Polygon *pPolygon);

int anj_config_polygon_get(IXML_Node *pNode, Polygon *pPolygon);

void anj_config_polygon_check(Polygon *pData);

int anj_config_zoom_exist(const char *filePath);

double anj_config_zoom_multile_get_by_xml();

void anj_config_zoom_multile_save(double multiple);

char *anj_config_pos_value_get(IXML_Document *pDoc, char *fieldName);

int anj_config_load_cust();

int anj_config_save_all(GlobalConfig *pCfg, const char *filename);

int anj_config_load(const char *pCfgName, void *pConfig, char *filePath);

int anj_config_get_default(char *pSrcFile, int iFilenameLen, GlobalConfig *pConfig);

int anj_config_copy_default(void);

int anj_config_cust_default_path(char *buf, int len);

int anj_config_init();

int anj_config_uninit();

int anj_config_save_node(char *pNodeXml, const char *pNodeIdStart, const char *pNodeIdEnd);

int anj_config_parse_user_cmd(char *xmlBuf);

char *anj_config_default_file_get();

int anj_config_parse(const char *data, GlobalConfig *pCfg);
int anj_config_parse_file(const char *pFileName, GlobalConfig *pCfg);

void *getGlbConfig(void);
void *getMediaConfig(void);
void *getMediaDefConfig(void);
void *getMediaStreamConfig(void);
void *getRecordConfig(void);
void *getNetWorkConfig(void);
void *getAlarmConfig(void);
void *getSystemConfig(void);
void *getGb28181Config(void);
void *getGat1400Config(void);
void *getServerConfig(void);
void *getPlatformConfig(void);
void *getVersionConfig(void);

void *getRWlock(void);

#ifdef __cplusplus
}
#endif
#endif
