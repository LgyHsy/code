#ifndef _ANJ_CONFIG_PTZ_H_
#define _ANJ_CONFIG_PTZ_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
    int HStep;
    int VStep;
} PtzStep;

typedef struct
{
    int HDir;
    int VDir;
} PtzDir;

typedef struct
{
    int HSpeed;
    int VSpeed;
} PtzSpeed;

#define PTZ_NAME_MAX 64
typedef struct
{
    int preset_id;           /*记录预置点号数值*/
    PtzStep preset_step;     /*记录从初始位到预置点的步数*/
    char name[PTZ_NAME_MAX]; /*预置点名字*/
    double zoom_multiple;
} PtzPreset;

typedef struct
{
    int enable;
    PtzStep left_margin;
    double left_multiple;
    PtzStep right_margin;
    double right_multiple;
} PtzLineScan;

typedef struct zoom_param
{
    double cur_multiple;
    double multiple_step;
    double max_multiple;
    double min_multiple;
} ZOOM_PARAM;

typedef struct
{
    PtzStep left_down_point;
    PtzStep right_up_point;
}Ptz3dOrientation;

#define MAX_PTZ_PRESET 255
typedef struct
{
    PtzPreset m_ptzPreset[MAX_PTZ_PRESET];
    PtzLineScan m_ptzLineScan;
    int m_ptzCruiseEnable;
    PtzStep m_resetStep;   /*断电重启复位位置*/
    PtzStep m_MaxStep;     /*云台可运行的最大步数*/
    PtzStep m_MistakeStep; /*云台机壳误差步数*/
    int watch_guard;       /*看守卫的预置点号*/
    int watch_guard_time;  /*看守位时间*/
    PtzDir m_ptzDir;
    Ptz3dOrientation m_3dOrientStep;    /*3D定位范围*/
    PtzSpeed m_ptzSpeed;
    ZOOM_PARAM m_zoom;
} IotPtzConfig;

int anj_config_ptz_load();
char *anj_ptz_config_conver_xml(IotPtzConfig *ptzCfg);
void anj_ptz_config_save(IotPtzConfig *pstIotPtzConfig);

IotPtzConfig *getIotPtzConfig();

#ifdef __cplusplus
}
#endif

#endif
