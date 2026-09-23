/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2010-2014, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef	__H_ONVIF_H__
#define	__H_ONVIF_H__

#include "sys_inc.h"


/***************************************************************************************/
#define ONVIF_TOKEN_LEN   	100
#define ONVIF_NAME_LEN    	100
#define ONVIF_URI_LEN     	300
#define ONVIF_SCOPE_LEN		128

#define PTZ_MAX_PRESETS     100 
#define MAX_DNS_SERVER		2
#define MAX_SEARCHDOMAIN	4
#define MAX_NTP_SERVER		2
#define MAX_SERVER_PORT		4
#define MAX_GATEWAY			2
#define MAX_RES_NUMS		50
#define MAX_SCOPE_NUMS		100


/***************************************************************************************/
typedef enum 
{
	ONVIF_OK = 0,
	ONVIF_ERR_INVALID_IPV4_ADDR = -1,
	ONVIF_ERR_INVALID_IPV6_ADDR = -2,
	ONVIF_ERR_INVALID_DNS_NAME = -3,
	ONVIF_ERR_SERVICE_NOT_SUPPORT = -4,
	ONVIF_ERR_PORT_ALREADY_INUSE = -5,
	ONVIF_ERR_INVALID_GATEWAY_ADDR = -6,
	ONVIF_ERR_INVALID_HOSTNAME = -7,
	ONVIF_ERR_MISSINGATTR = -8,
	ONVIF_ERR_INVALID_DATETIME = -9,
	ONVIF_ERR_INVALID_TIMEZONE = -10,
	ONVIF_ERR_PROFILE_EXISTS = -11,
	ONVIF_ERR_MAX_NVT_PROFILES = -12,
	ONVIF_ERR_NO_PROFILE = -13,
	ONVIF_ERR_DEL_FIX_PROFILE = -14,
	ONVIF_ERR_NO_CONFIG = -15,
	ONVIF_ERR_NO_PTZ_PROFILE = -16,
	ONVIF_ERR_NO_HOME_POSITION = -17,
	ONVIF_ERR_NO_TOKEN = - 18,
	ONVIF_ERR_PRESET_EXIST = -19,
	ONVIF_ERR_TOO_MANY_PRESETS = -20,
	ONVIF_ERR_MOVING_PTZ = -21,
	ONVIF_ERR_NO_ENTITY = -22,
	ONVIF_ERR_INVALID_NETWORK_INTERFACE = -23, 
	ONVIF_ERR_INVALID_MTU_VALUE = -24,
	ONVIF_ERR_CONFIG_MODIFY = -25,
	ONVIF_ERR_CONFIGURATION_CONFLICT = -26,
	ONVIF_ERR_INVALID_POSIION = -27,
	ONVIF_ERR_TOO_MANY_SCOPES = -28,
	ONVIF_ERR_FIXED_SCOPE = -29,
	ONVIF_ERR_NO_SCOPE = -30,
	ONVIF_ERR_SCOPE_OVERWRITE = -31,
	ONVIF_ERR_RESOURCE_UNKNOWN_FAULT = -32,
	ONVIF_ERR_NO_SOURCE = -33,
	ONVIF_ERR_CANNOT_OVERWRITE_HOME = -34,
	ONVIF_ERR_SETTINGS_INVALID = -35,
	ONVIF_ERR_NO_IMAGEING_FOR_SOURCE = -36,
} ONVIF_RET;

typedef enum 
{   
	VIDEO_ENCODING_JPEG = 0,
	VIDEO_ENCODING_MPEG4 = 1,
	VIDEO_ENCODING_H264 = 2,
	VIDEO_ENCODING_H265 = 3
} E_VIDEO_ENCODING;

typedef enum 
{   
    AUDIO_ENCODING_G711 = 0,
    AUDIO_ENCODING_G726,
    AUDIO_ENCODING_AAC,
    AUDIO_ENCODING_G711A
} E_AUDIO_ENCODING;

typedef enum 
{
    H265_PROFILE_Baseline = 0, 
    H265_PROFILE_Main = 1, 
    H265_PROFILE_Extended = 2, 
    H265_PROFILE_High = 3
} E_H265_PROFILE;

typedef enum 
{
    H264_PROFILE_Baseline = 0, 
    H264_PROFILE_Main = 1, 
    H264_PROFILE_Extended = 2, 
    H264_PROFILE_High = 3
} E_H264_PROFILE;

typedef enum 
{
    MPEG4_PROFILE_SP = 0, 
    MPEG4_PROFILE_ASP = 1
} E_MPEG4_PROFILE;

typedef enum 
{
	CAP_CATEGORY_INVALID = -1,
	CAP_CATEGORY_MEDIA,
	CAP_CATEGORY_DEVICE,
	CAP_CATEGORY_ANALYTICS,
	CAP_CATEGORY_EVENTS,
	CAP_CATEGORY_IMAGE,
	CAP_CATEGORY_PTZ,
	CAP_CATEGORY_ALL
} E_CAP_CATEGORY;

typedef enum
{
	PTZ_STA_IDLE,
	PTZ_STA_MOVING,
	PTZ_STA_UNKNOWN
} E_PTZ_STATUS;

/***************************************************************************************/

typedef struct 
{
	int x_min;
	int x_max;
	int y_min;
	int y_max;
	int w_min;
	int w_max;
	int h_min;
	int h_max;
} ONVIF_V_SRC_CFG;

typedef struct
{
	int w;
	int h;
	char encoding[ONVIF_NAME_LEN];
	int channle;
	
	int gov_length_min;
	int gov_length_max;
	
	int frame_rate_min;
	int frame_rate_max;
	
	int bit_rate_range_min;
	int bit_rate_range_max;
	
	int encoding_interval_min;
	int encoding_interval_max;
} RESOLUTION;

typedef struct
{
	int frame_rate_min;
	int frame_rate_max;
	int encoding_interval_min;
	int encoding_interval_max;
} JPEG_CFG_OPT;

typedef struct
{
	int sp_profile 	: 1;
	int asp_profile : 1;
	int reserverd	: 30;
	
	int gov_length_min;
	int gov_length_max;
	int frame_rate_min;
	int frame_rate_max;
	int encoding_interval_min;
	int encoding_interval_max;
} MPEG4_CFG_OPT;

typedef struct
{
	int baseline_profile	: 1;
	int main_profile 		: 1;
	int extended_profile 	: 1;
	int high_profile 		: 1;
	int reserverd			: 28;
	
	int gov_length_min;
	int gov_length_max;
	int frame_rate_min;
	int frame_rate_max;
	int encoding_interval_min;
	int encoding_interval_max;
} H264_CFG_OPT;

typedef struct
{
	int baseline_profile	: 1;
	int main_profile 		: 1;
	int extended_profile 	: 1;
	int high_profile 		: 1;
	int reserverd			: 28;
	
	int gov_length_min;
	int gov_length_max;
	int frame_rate_min;
	int frame_rate_max;
	int encoding_interval_min;
	int encoding_interval_max;
} H265_CFG_OPT;

typedef struct
{
	int baseline_profile	: 1;
	int main_profile 		: 1;
	int extended_profile 	: 1;
	int high_profile 		: 1;
	int reserverd			: 28;
	
	int frame_rate_min;
	int frame_rate_max;
	int encoding_interval_min;
	int encoding_interval_max;
} H265P_CFG_OPT;


typedef struct 
{
	RESOLUTION resolution[MAX_RES_NUMS];
	int resolution_num;
	
	JPEG_CFG_OPT 	jpeg_opt;
	MPEG4_CFG_OPT	mpeg4_opt;
	H264_CFG_OPT	h264_opt;
	H265_CFG_OPT	h265_opt;
	H265P_CFG_OPT	h265P_opt;
	
	int h264_sup;
	int h265_sup;
	int h265p_sup;
	int mjpg_sup;
	int quality_min;
	int quality_max;
	int gov_length_min;
	int gov_length_max;
	int frame_sup_min;
	int frame_sup_max;
	
} ONVIF_V_ENC_CFG;

typedef struct _ONVIF_V_SRC
{
	struct _ONVIF_V_SRC * next;
	
    // Bounds   
    int  width;
    int  height;
    int  x;
    int  y;
    
    int  use_count;
    char token[ONVIF_TOKEN_LEN];
    char name[ONVIF_NAME_LEN];
    char source_token[ONVIF_TOKEN_LEN]; 
} ONVIF_V_SRC;

typedef struct _ONVIF_V_ENC
{    
	struct _ONVIF_V_ENC * next;	

	char name[ONVIF_NAME_LEN];
    char token[ONVIF_TOKEN_LEN];

	int  use_count;
	
    int  width;
    int  height;
    
	int  quality;
    int  session_timeout;
    int  framerate_limit;
	int  encoding_interval;
	int  bitrate_limit;

	E_VIDEO_ENCODING encoding;
	
	int  gov_len;
	int  profile;
} ONVIF_V_ENC;


typedef struct _ONVIF_A_SRC
{    
	struct _ONVIF_A_SRC * next;
	
    int  use_count;
    char token[ONVIF_TOKEN_LEN];
	char name[ONVIF_NAME_LEN];
    char source_token[ONVIF_TOKEN_LEN]; 
} ONVIF_A_SRC;

typedef struct _ONVIF_A_ENC
{
	struct _ONVIF_A_ENC * next;
	
    int  use_count;
    int  session_timeout;
    
    char name[ONVIF_NAME_LEN];
    char token[ONVIF_TOKEN_LEN];
    
	int  sample_rate;
	int  bitrate;    

	E_AUDIO_ENCODING encoding;
} ONVIF_A_ENC;

typedef struct
{
	float min;
	float max;
} ONVIF_FRANGE;

typedef struct
{
    BOOL    used_flag;
    
    char	name[ONVIF_NAME_LEN];
    char	token[ONVIF_TOKEN_LEN];

    float 	pantilt_pos_x;
	float 	pantilt_pos_y;
	float   zoom_pos;
} PTZ_PRESET;

typedef struct
{
	char	name[ONVIF_NAME_LEN];
    char	token[ONVIF_TOKEN_LEN];
	
	float 	def_pantilt_speed_x;
	float 	def_pantilt_speed_y;
	float 	def_zoom_speed;
	uint32 	def_timeout;

	ONVIF_FRANGE pantilt_limits_x;
	ONVIF_FRANGE pantilt_limits_y;
	ONVIF_FRANGE zoom_limits;
	ONVIF_FRANGE timeout_range;

	PTZ_PRESET   presets[PTZ_MAX_PRESETS];
} PTZ_CFG;

typedef struct
{
	float pantilt_pos_x;
	float pantilt_pos_y;
	float zoom_pos;
	
	E_PTZ_STATUS move_sta;
	E_PTZ_STATUS zoom_sta;

	char  error[200];
	char  utc_time[64];
} PTZ_STATUS;

typedef struct _PTZ_NODE
{
	struct _PTZ_NODE * next;

	unsigned int fixed_home_pos 		: 1;
	unsigned int abs_pantilt_space 		: 1;
	unsigned int abs_zoom_space 		: 1;
	unsigned int rel_pantilt_space 		: 1;
	unsigned int rel_zoom_space 		: 1;	
	unsigned int con_pantilt_space 		: 1;
	unsigned int con_zoom_space 		: 1;
	unsigned int pantile_speed_space	: 1;
	unsigned int zoom_speed_space		: 1;

	unsigned int reserved				: 23;

	char 		 name[ONVIF_NAME_LEN];
    char         token[ONVIF_TOKEN_LEN];

	ONVIF_FRANGE abs_pantilt_x;
	ONVIF_FRANGE abs_pantilt_y;
	ONVIF_FRANGE abs_zoom;
	ONVIF_FRANGE rel_pantilt_x;
	ONVIF_FRANGE rel_pantilt_y;
	ONVIF_FRANGE rel_zoom;
	ONVIF_FRANGE con_pantilt_x;
	ONVIF_FRANGE con_pantilt_y;
	ONVIF_FRANGE con_zoom;
	ONVIF_FRANGE pantile_speed;
	ONVIF_FRANGE zoom_speed;

	PTZ_CFG		 config;
} PTZ_NODE;

typedef struct _ONVIF_PROFILE
{ 
	struct _ONVIF_PROFILE * next;
	
	ONVIF_V_SRC * video_src;
	ONVIF_V_ENC * video_enc;
	ONVIF_A_SRC * audio_src;
	ONVIF_A_ENC * audio_enc;
	PTZ_NODE    * ptz_node;

	char name[ONVIF_NAME_LEN];
	char token[ONVIF_TOKEN_LEN];
	char stream_uri[ONVIF_URI_LEN];
	BOOL fixed;

} ONVIF_PROFILE;

typedef struct _ONVIF_SCOPE
{
	char scope[ONVIF_SCOPE_LEN];
	BOOL fixed;	
} ONVIF_SCOPE;

typedef struct _ONVIF_NET_INF
{
	struct _ONVIF_NET_INF * next;
	
	char name[ONVIF_NAME_LEN];
	char token[ONVIF_TOKEN_LEN];

	BOOL enabled;
	char hwaddr[32];
	int	 mtu;
	BOOL ipv4_enabled;
	BOOL fromdhcp;
	char ipv4_addr[32];
	int	 prefix_len;
} ONVIF_NET_INF;

typedef struct 
{
    unsigned int hostname_fromdhcp          : 1;
    unsigned int set_hostname_need_reboot   : 1;
	unsigned int dns_fromdhcp				: 1;
	unsigned int ntp_fromdhcp				: 1;
	unsigned int http_support				: 1;
	unsigned int http_enable				: 1;
	unsigned int https_support				: 1;
	unsigned int https_enable				: 1;
	unsigned int rtsp_support				: 1;
	unsigned int rtsp_enable				: 1;
	
    unsigned int reserved                   : 22;
    
    char    hostname[100];
	char	dns_searchdomain[MAX_SEARCHDOMAIN][64];
	char	dns_server[MAX_DNS_SERVER][32];
	char	ntp_server[MAX_NTP_SERVER][32];
	char	gateway[MAX_GATEWAY][32];

	int		http_port[MAX_SERVER_PORT];
	int		https_port[MAX_SERVER_PORT];
	int		rtsp_port[MAX_SERVER_PORT];

	ONVIF_NET_INF * interfaces;
} ONVIF_NET;

typedef struct
{
	int	BacklightCompensation_Mode;	// 0-OFF, 1-ON
	int Brightness;
	int ColorSaturation;
	int Contrast;
	int	Exposure_Mode;				// 0-AUTO, 1-MANUAL			
	int MinExposureTime;
	int MaxExposureTime;
	int MinGain;
	int MaxGain;
	int IrCutFilter_Mode;			// 0-OFF, 1-ON, 2-AUTO
	int Sharpness;
	int	WideDynamicRange_Mode;		// 0-OFF, 1-ON
	int WideDynamicRange_Level;
	int	WhiteBalance_Mode;			// 0-AUTO, 1-MANUAL
} IMAGE_CFG;

typedef struct
{
	unsigned int 	BacklightCompensation_ON	: 1;
	unsigned int 	BacklightCompensation_OFF	: 1;
	unsigned int    Exposure_AUTO				: 1;
	unsigned int    Exposure_MANUAL				: 1;
	unsigned int 	AutoFocus_AUTO				: 1;
	unsigned int 	AutoFocus_MANUAL			: 1;
	unsigned int 	IrCutFilter_ON				: 1;
	unsigned int 	IrCutFilter_OFF				: 1;
	unsigned int 	IrCutFilter_AUTO			: 1;
	unsigned int 	WideDynamicRange_ON			: 1;
	unsigned int 	WideDynamicRange_OFF		: 1;
	unsigned int 	WhiteBalance_AUTO			: 1;
	unsigned int 	WhiteBalance_MANUAL			: 1;
	
	unsigned int	reserved     				: 19;

	int				Brightness_min;
	int				Brightness_max;
	int				ColorSaturation_min;
	int				ColorSaturation_max;
	int				Contrast_min;
	int				Contrast_max;
	int				MinExposureTime_min;
	int				MinExposureTime_max;
	int				MaxExposureTime_min;
	int				MaxExposureTime_max;
	int				MinGain_min;
	int				MinGain_max;
	int				MaxGain_min;
	int				MaxGain_max;
	int				Sharpness_min;
	int				Sharpness_max;
	int				WideDynamicRange_Level_min;
	int				WideDynamicRange_Level_max;
} IMAGE_OPTIONS;

#define LARGE_INFO_LENGTH 1024
#define AJ_OEM_STR_LEN 32
#define MACH_ADDR_LENGTH 6
#define DEFAULT_HOST_NAME "IPNC"
#define DEFAULT_ONVIF_NAME "ONVIF_ICAMERA"
#define DEFAULT_ONVIF_NONE "NONE"
#define EXIST 1
#define NOT_EXIST 0

// NET_IPV4 already defined in anj_mw_net.h (included via compat shim)
// typedef union __NET_IPV4 {
// 	unsigned long	int32;
// 	char str[4];
// } NET_IPV4;

typedef struct 
{
	unsigned int 	discoverable 	: 1;
	unsigned int 	need_auth	 	: 1;
	unsigned int	evt_sim_flag 	: 1;	/* event simulate flag */
	unsigned int	ptz_enable	 	: 1;
	unsigned int    merge_src	 	: 1;	/* merge source flag */
	unsigned int	datetime_ntp 	: 1;	/* update system datetime from ntp */
	unsigned int    daylightsavings	: 1;	/* DaylightSavings */
	unsigned int	reserved     	: 27;

	char 			serv_ip[32];
	unsigned short 	serv_port;
	
	char 			auth_user[32];
	char 			auth_pass[32];		

	/* device information */
	char			manufacturer[LARGE_INFO_LENGTH];
	char			model[LARGE_INFO_LENGTH];
	char			firmware_version[LARGE_INFO_LENGTH];
	char			serial_number[LARGE_INFO_LENGTH];
	char			hardware_id[LARGE_INFO_LENGTH];

	char			timezone[32];
	
	int  			evt_renew_time;
	int	 			evt_sim_interval;	/* event simulate interval */

	ONVIF_V_SRC_CFG video_src_cfg;
	ONVIF_V_ENC_CFG video_enc_cfg;
	
	ONVIF_V_SRC   * video_src;
	ONVIF_A_SRC   * audio_src;
	ONVIF_V_ENC   * video_enc;
	ONVIF_A_ENC   * audio_enc;

	ONVIF_SCOPE   	scopes[MAX_SCOPE_NUMS];	
	ONVIF_PROFILE * profiles;	
	PTZ_NODE      * ptznodes;

	ONVIF_NET       network;
	
	IMAGE_CFG		img_cfg;
	IMAGE_OPTIONS	img_opt;

	int enablethird;
} ONVIF_CFG;


typedef struct
{
	unsigned int	sys_timer_run	: 1;	/* timer running flag */
	unsigned int    discovery_flag	: 1;	/* discovery flag */	
	
	unsigned int	reserved		: 30;
	
	int	 			v_src_idx;
	int	 			a_src_idx;
	int	 			v_enc_idx;
	int	 			a_enc_idx;
	int  			profile_idx;
	int  			ptznode_idx;
	int             preset_idx;
	int				netinf_idx;

	char 			local_ipstr[32];
	int  			local_port;

	PPSN_CTX      * eua_fl;
	PPSN_CTX      * eua_ul;

	uint32 	   		timer_id;

	int		   		discovery_fd;
	pthread_t  		discovery_tid;
} ONVIF_CLS;



#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <termios.h>
#include <linux/soundcard.h>
#include <poll.h>
#include <fcntl.h>
#include <netdb.h>

#include "curl/curl.h"
#include "cJSON.h"
// ipc1 headers replaced by ipc2 compat shim
// #include "debug_util.h"
// #include "msg_def.h"
// #include "bufmgr.h"
// #include "tps_msg.h"
// #include "msg_util.h"
// #include "func_util.h"
// #include "system_msg.h"
// #include "sys_info.h"
// #include "aux_msg.h"
// #include "net_config.h"
// #include "misc_util.h"
// #include "media_inf.h"
#include "onvif_compat_shim.h"
/***************************************************************************************/

void 			onvif_init();
void 			onvif_init_cfg();
void 			onvif_init_profile();

/***************************************************************************************/
ONVIF_RET 		onvif_add_scopes(ONVIF_SCOPE * p_scope, int scope_max);
ONVIF_RET 		onvif_set_scopes(ONVIF_SCOPE * p_scope, int scope_max);
ONVIF_RET 		onvif_remove_scopes(ONVIF_SCOPE * p_scope, int scope_max);
BOOL 			onvif_is_scope_exist(const char * scope);
int 			onvif_add_scope(const char * scope, BOOL fixed);
ONVIF_SCOPE   * onvif_get_idle_scope();


/***************************************************************************************/
ONVIF_PROFILE * onvif_find_profile(const char * token);
ONVIF_PROFILE * onvif_add_profile(BOOL fixed);
ONVIF_V_SRC   * onvif_find_video_source(const char * token);
ONVIF_A_SRC   * onvif_find_audio_source(const char * token);
ONVIF_V_ENC   * onvif_find_video_encoder(const char * token);
ONVIF_A_ENC   * onvif_find_audio_encoder(const char * token);
PTZ_NODE      * onvif_find_ptz_node(const char * token);
PTZ_NODE      * onvif_find_ptz_cfg(const char * token);
PTZ_PRESET    * onvif_find_preset(const char * profile_token, const char  * preset_token);
PTZ_PRESET    * onvif_get_idle_preset(const char * profile_token);
ONVIF_NET_INF * onvif_find_net_inf(const char * token);

const char    * onvif_get_audio_encoding_str(E_AUDIO_ENCODING encoding);
const char    * onvif_get_video_encoding_str(E_VIDEO_ENCODING encoding);
const char    * onvif_get_h264_profile_str(E_H264_PROFILE profile);
const char    * onvif_get_mpeg4_profile_str(E_MPEG4_PROFILE profile);

const char    * onvif_get_ptz_status_str(E_PTZ_STATUS ptz_status);

E_CAP_CATEGORY	onvif_get_cap_category(const char * cateory);

#ifdef __cplusplus
}
#endif

#endif


