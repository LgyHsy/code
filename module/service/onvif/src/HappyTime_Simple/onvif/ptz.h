#ifndef PTZ_H
#define PTZ_H
#if defined (__cplusplus)
extern "C" {
#endif



#define LENS_UP      			0x01 /*向上转*/
#define LENS_LEFT    			0x02 /*向左转*/
#define LENS_RIGHT     			0x03 /*向右转*/
#define LENS_DOWN     			0x04 /*向下转*/
#define LENS_FAR     			0x05 /*镜头拉远*/
#define LENS_NEAR     			0x06 /*镜头拉近*/
#define LENS_FOCUSNEAR    		0x07 /*聚焦*/
#define LENS_FOCUSFAR    		0x08 /*散焦*/
#define LENS_DIAPHRAGM_LARGE  	0x09 /*光圈增大（变亮）*/
#define LENS_DIAPHRAGM_SMALL  	0x0a /*光圈减小（变暗）*/
#define LENS_PRESET_GOTO   		0x0b /*切换到预置点*/
#define LENS_PRESET_SET    		0x0c /*设置预置点*/
#define LENS_PRESET_DEL    		0x0d /*删除预置点*/
#define LENS_LIGHT_ON    		0x0e /*打开灯光*/
#define LENS_LIGHT_OFF    		0x0f /*关闭灯光*/
#define LENS_AUTO     			0x10 /*自动*/
#define LENS_STOP        		0x11 /*停止动作*/

//new added 20120427
#define WIPER_PWRON  			0x12 /*接通雨刷开关*/
#define WIPER_PWROFF 			0x13 /*关闭雨刷开关*/

#define LENS_LEFT_UP			0x14//左上
#define LENS_LEFT_DOWN			0x15//左下
#define LENS_RIGHT_UP			0x16//右上
#define LENS_RIGHT_DOWN			0x17//右下
//int CPlatformApp::PtzCmdHandle(int cmd, int panSpeed,int tiltSpeed,char *msgBody);
//int PtzCmdHandle(int cmd, int panSpeed,int tiltSpeed,int presetId,char *msgBody);
int PtzCmdHandle(int cmd, int panSpeed,int tiltSpeed,int presetId);
int goto_home_preset(char *profileToken);
int set_home_preset(char *profileToken);
int goto_preset(char *profileToken, char *presetToken);
int remove_preset(char *profileToken, char *presetToken);

int add_preset(char *profileToken, char *presetName_onvif, char *presetToken_onvif, char *responseToken, int isHome);
void show_preset_list(void);
int save_onvif_ptz(void);
int load_onvif_ptz(void);
void ptz_mutex_init();
void ptz_mutex_lock();
void ptz_mutex_unlock();
void ptz_mutex_deinit();

int atoi_plus(char *str);

#if defined (__cplusplus)
}
#endif



#endif
