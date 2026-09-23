/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
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
#include "onvif_cm.h"
#include "linked_list.h"
#include "hqueue.h"
#include "http.h"
#ifdef HTTPD
#include "httpd.h"
#endif
#include "anj_mw_ring_queue.h"

/***************************************************************************************/
#define ONVIF_MSG_SRC	    1
#define ONVIF_TIMER_SRC	    2
#define ONVIF_DEL_UA_SRC    3
#define ONVIF_EXIT          4

#ifndef HTTP_GET
#define HTTP_GET            5
#endif
#ifndef HTTP_PUT
#define HTTP_PUT            6
#endif

#ifdef ANDROID
#define RTSP_URL_SUFFIX     "videodevice+audiodevice" // back camera
#else
#define RTSP_URL_SUFFIX     "test.mp4"
#endif

/***************************************************************************************/

// video source list
typedef struct _VideoSourceList
{
	struct _VideoSourceList * next;
	
	onvif_VideoSource       VideoSource; 
	onvif_VideoSourceMode	VideoSourceMode;

#ifdef IMAGE_SUPPORT
	onvif_ImagingSettings   ImagingSettings;
	onvif_ImagingOptions    ImagingOptions;
	ImagingPresetList     * Presets;
	char                    CurrentPresetToken[ONVIF_TOKEN_LEN];
#endif

#ifdef THERMAL_SUPPORT
	BOOL                                    ThermalSupport;
	onvif_ThermalConfiguration              ThermalConfiguration;
	onvif_ThermalConfigurationOptions       ThermalConfigurationOptions;
	onvif_RadiometryConfiguration           RadiometryConfiguration;
	onvif_RadiometryConfigurationOptions    RadiometryConfigurationOptions;
#endif
} VideoSourceList;

// video source mode list
typedef struct _VideoSourceModeList
{
    struct _VideoSourceModeList * next;

	onvif_VideoSourceMode	VideoSourceMode;  	
} VideoSourceModeList;

// video source configuration list
typedef struct _VideoSourceConfigurationList
{
	struct _VideoSourceConfigurationList * next;

	onvif_VideoSourceConfiguration          Configuration;
	onvif_VideoSourceConfigurationOptions   Options;
} VideoSourceConfigurationList;

// video encoder configuration list
typedef struct _VideoEncoderConfigurationList
{    
	struct _VideoEncoderConfigurationList * next;	

	onvif_VideoEncoderConfiguration	Configuration;	
} VideoEncoderConfigurationList;

// audio source list
typedef struct _AudioSourceList
{    
	struct _AudioSourceList * next;
	
    onvif_AudioSource AudioSource;	
} AudioSourceList;

// audio source configuration list
typedef struct _AudioSourceConfigurationList
{    
	struct _AudioSourceConfigurationList * next;
	
    onvif_AudioSourceConfiguration 	Configuration;	
} AudioSourceConfigurationList;

// audio encoder configuration list
typedef struct _AudioEncoderConfigurationList
{
	struct _AudioEncoderConfigurationList * next;
	
    onvif_AudioEncoderConfiguration Configuration;
} AudioEncoderConfigurationList;

typedef struct _MetadataConfigurationList
{
	struct _MetadataConfigurationList * next;
	
	onvif_MetadataConfiguration Configuration;
} MetadataConfigurationList;

// ptz preset list
typedef struct _PTZPresetList
{
	struct _PTZPresetList * next;

	onvif_PTZPreset	PTZPreset;
} PTZPresetList;

// ptz configuration list
typedef struct _PTZConfigurationList
{
	struct _PTZConfigurationList * next;
	
	onvif_PTZConfiguration          Configuration;
	onvif_PTZConfigurationOptions   Options;
} PTZConfigurationList;

// ptz node list
typedef struct _PTZNodeList
{
	struct _PTZNodeList * next;

	onvif_PTZNode	PTZNode;	
} PTZNodeList;

// preset tour list
typedef struct _PresetTourList
{
	struct _PresetTourList * next;

	onvif_PresetTour    PresetTour;
} PresetTourList;

// video analytics configuration list
typedef struct _VideoAnalyticsConfigurationList
{
	struct _VideoAnalyticsConfigurationList	* next;

	onvif_SupportedRules                SupportedRules;			    // supported rules
	onvif_SupportedAnalyticsModules     SupportedAnalyticsModules;  // supported analytics modules
	
	onvif_VideoAnalyticsConfiguration   Configuration;
} VideoAnalyticsConfigurationList;

// network interface list
typedef struct _NetworkInterfaceList
{
	struct _NetworkInterfaceList * next;
	
	onvif_NetworkInterface	NetworkInterface; 
} NetworkInterfaceList;

typedef struct 
{
	uint32 NetworkProtocolFlag      : 1;
	uint32 DNSInformationFlag       : 1;
	uint32 NTPInformationFlag       : 1;
	uint32 HostnameInformationFlag  : 1;
	uint32 NetworkGatewayFlag       : 1;
	uint32 DiscoveryModeFlag        : 1;
	uint32 ZeroConfigurationFlag    : 1;
	uint32 DynamicDNSInformationFlag: 1;
	uint32 Reserved                 : 24;

	onvif_NetworkProtocol		    NetworkProtocol;
	onvif_DNSInformation		    DNSInformation;
	onvif_NTPInformation		    NTPInformation;
	onvif_HostnameInformation	    HostnameInformation;
	onvif_NetworkGateway		    NetworkGateway;
	onvif_DiscoveryMode			    DiscoveryMode;
	onvif_NetworkZeroConfiguration  ZeroConfiguration;
	onvif_DynamicDNSInformation     DynamicDNSInformation;
	
	NetworkInterfaceList          * interfaces;
} ONVIF_NET;

// osd configuration list
typedef struct _OSDConfigurationList
{
	struct _OSDConfigurationList * next;
	
	onvif_OSDConfiguration OSD;
} OSDConfigurationList;

typedef struct _RecordingList
{
	struct _RecordingList * next;

	onvif_Recording	Recording;

	time_t	EarliestRecording;
	time_t	LatestRecording;
	
	onvif_RecordingStatus	RecordingStatus;
} RecordingList;

typedef struct _RecordingJobList
{	
	struct _RecordingJobList * next;

	onvif_RecordingJob	RecordingJob;
} RecordingJobList;

typedef struct _NotificationMessageList
{
	struct _NotificationMessageList * next;

    int     refcnt;     // reference count

	onvif_NotificationMessage	NotificationMessage;
} NotificationMessageList;

typedef struct _ONVIF_PROFILE
{
	struct _ONVIF_PROFILE * next;

	VideoSourceConfigurationList    * v_src_cfg;        // video source configuration
	VideoEncoder2ConfigurationList  * v_enc_cfg;        // video encoder configuration

#ifdef AUDIO_SUPPORT    
	AudioSourceConfigurationList    * a_src_cfg;        // audio source configuration
	AudioEncoder2ConfigurationList  * a_enc_cfg;        // audio encoder configuration
	AudioDecoderConfigurationList   * a_dec_cfg;        // audio decoder configuration
#endif

#ifdef PTZ_SUPPORT
	PTZConfigurationList            * ptz_cfg;          // ptz configuration
	PTZPresetList                   * presets;          // ptz presets list
	PresetTourList                  * preset_tour;      // preset tour
#endif

	MetadataConfigurationList       * metadata_cfg;     // metadata configuration

#ifdef VIDEO_ANALYTICS
	VideoAnalyticsConfigurationList * va_cfg;	        // video analytics configuration
#endif

#ifdef DEVICEIO_SUPPORT
	AudioOutputConfigurationList    * a_output_cfg;     // audio output configuration
#endif

	char        name[ONVIF_NAME_LEN];				    // profile name
	char        token[ONVIF_TOKEN_LEN];				    // profile token
	char        stream_uri[ONVIF_URI_LEN];			    // rtsp stream address
	BOOL        fixed;							        // fixed profile flag	
	BOOL        multicasting;						    // sending multicast streaming flag
	BOOL        append_params;                          // Append audio and video encoding parameters to the end of the RTSP stream address
} ONVIF_PROFILE;

typedef struct 
{
	uint32 	    need_auth	 	        : 1;		    // Whether need auth request flag
	uint32	    evt_sim_flag 	        : 1;	        // event simulate flag
	uint32      log_enable              : 1;		    // Whether log enable flag
	uint32 	    http_enable	            : 1;		    // whether http enable flag
	uint32 	    https_enable	        : 1;		    // whether https enable flag
	uint32      SystemDateTimeFlag      : 1;            // Is the SystemDateTime field initialized 
	uint32      DeviceInformationFlag   : 1;            // Is the DeviceInformation field initialized 
	uint32      UsersFlag               : 1;            // Is the users field initialized 
	uint32      ScopesFlag              : 1;            // Is the scopes field initialized 
	uint32      md5_hashing             : 1;            // MD5 hash algorithm
	uint32      sha256_hashing          : 1;            // SHA-256 hash algorithm
	uint32	    reserved     	        : 21;

	char        server_ip[128];					        // server ip
	uint16 	    http_port;                              // http server port
	uint16 	    https_port;                             // https server port
	int         http_max_users;                         // max http connection clients 
	char        cert_file[256];                         // cert file
	char        key_file[256];                          // key file
	int         log_level;                              // log level, ses sys_log.h
	
	onvif_User  users[MAX_USERS];                       // user configurations	
	onvif_Scope scopes[MAX_SCOPE_NUMS];                 // device scopes
	ONVIF_NET   network;                                // network information
	
	char        EndpointReference[64];                  // endpoint reference
	
	int         evt_renew_time;                         // event renew interval, unit is second

#ifdef PROFILE_Q_SUPPORT
	int         device_state;                           // 0 - Factory Default state, 1 - Operational State
	                                                    // Factory Default State requires WS-Discovery, DHCP and IPv4 Link Local Address to be enabled by default
	                                                    // A device shall provide full anonymous access to all ONVIF commands while the device operates in Factory Default State
#endif

	/********************************************************/
	VideoSourceList   		              * v_src;
	VideoSourceConfigurationList	      * v_src_cfg;
	VideoEncoder2ConfigurationList	      * v_enc_cfg;

#ifdef AUDIO_SUPPORT	
	AudioSourceList   		              * a_src;
	AudioSourceConfigurationList 	      * a_src_cfg;
	AudioEncoder2ConfigurationList        * a_enc_cfg;
	AudioDecoderConfigurationList         * a_dec_cfg;
#endif

	ONVIF_PROFILE 		                  * profiles;
	OSDConfigurationList                  * OSDs;
	MetadataConfigurationList	          * metadata_cfg;

#ifdef MEDIA2_SUPPORT
	MaskList                              * mask;
	onvif_MaskOptions                       MaskOptions;
#endif

#ifdef PTZ_SUPPORT
	PTZNodeList                           * ptz_node;
	PTZConfigurationList	              * ptz_cfg;
#endif

#ifdef VIDEO_ANALYTICS
	VideoAnalyticsConfigurationList       * va_cfg;
#endif

#ifdef PROFILE_G_SUPPORT
	RecordingList                         * recordings;
	RecordingJobList                      * recording_jobs;
	int 	                                replay_session_timeout;
#endif

#ifdef PROFILE_C_SUPPORT
	AccessPointList						  * access_points;
	DoorList                              * doors;
	AreaList                              * areas;
#endif

#ifdef DEVICEIO_SUPPORT
	VideoOutputList						  * v_output;
	VideoOutputConfigurationList          * v_output_cfg;
	AudioOutputList                       * a_output;
	AudioOutputConfigurationList          * a_output_cfg;
	RelayOutputList                       * relay_output;
	DigitalInputList                      * digit_input;
	SerialPortList                        * serial_port;
#endif

	/********************************************************/
	onvif_DeviceInformation					DeviceInformation;
	onvif_Capabilities						Capabilities;
	onvif_SystemDateTime					SystemDateTime;
	onvif_RemoteUser						RemoteUser;

	onvif_MetadataConfigurationOptions		MetadataConfigurationOptions;
	onvif_OSDConfigurationOptions			OSDConfigurationOptions;

#ifdef CREDENTIAL_SUPPORT
	CredentialList                        * credential;
	CredentialIdentifierItemList          * whiltlist;
	CredentialIdentifierItemList          * blacklist;
#endif

#ifdef ACCESS_RULES
	AccessProfileList                     * access_rules;
#endif

#ifdef SCHEDULE_SUPPORT
	ScheduleList                          * schedule;
	SpecialDayGroupList                   * specialdaygroup;
#endif

#ifdef RECEIVER_SUPPORT
	ReceiverList                          * receiver;
#endif

#ifdef IPFILTER_SUPPORT
	onvif_IPAddressFilter                   ipaddr_filter;
#endif

#ifdef STORAGE_SUPPORT
	StorageConfigurationList              * storage;
#endif

#ifdef GEOLOCATION_SUPPORT
	LocationEntityList                    * location;
#endif
} ONVIF_CFG;


typedef struct
{
	uint32      sys_timer_run	: 1;    // timer running flag
	uint32      discovery_flag	: 1;    // discovery flag
	uint32      reserved		: 30;

	char        server_ip[128];         // server ip
	uint16 	    http_port;              // http server port
	HTTPSRV     http_srv;               // http server

#ifdef HTTPS
	uint16 	    https_port;             // https server port
	HTTPSRV     https_srv;              // https server
#endif

	PPSN_CTX  * eua_fl;                 // event subscriber free list    
	PPSN_CTX  * eua_ul;                 // event subscriber used list  
	
	pthread_t   timer_id;               // timer id

	SOCKET      discovery_fd;           // discovery socket handler
	pthread_t   discovery_tid;          // discovery task handler

	HQUEUE    * msg_queue;              // message receive queue
	pthread_t   tid_main;               // main task thread

	PPSN_CTX *  timer_fl;               // timer free list
	PPSN_CTX *  timer_ul;               // timer used list

	HD_AUTH_INFO  onvif_auth;           // onvif auth info

#ifdef PROFILE_G_SUPPORT
	LINKED_LIST * search_list;          // search ua list
#endif

#ifdef HTTPD
	HTTPD_CLASS * httpd;                // httpd class
#endif
} ONVIF_CLS;

typedef struct
{
	int         v_src_idx;              // video source index	
	int         v_src_cfg_idx;          // video source configuration index	
	int         v_enc_idx;              // video encoder index	
	int         profile_idx;	        // profile index
	int         netinf_idx;             // network interface index
	int         osd_idx;                // osd index
	int         metadata_idx;           // metadata index

#ifdef MEDIA2_SUPPORT
	int         mask_idx;               // mask index
#endif

#ifdef IMAGE_SUPPORT
	int         image_preset_idx;       // image preset index
#endif

#ifdef AUDIO_SUPPORT
	int         a_src_idx;              // audio source  index
	int         a_src_cfg_idx;          // audio source configuration index
	int         a_enc_idx;              // audio encoder index
	int         a_dec_idx;              // audio decoder index
#endif

#ifdef PTZ_SUPPORT
	int         ptznode_idx;            // ptz node index
	int         ptzcfg_idx;             // ptz configuration index
	int         preset_idx;             // preset index
	int         preset_tour_idx;        // preset tour index
#endif

#ifdef PROFILE_G_SUPPORT
	int         recording_idx;          // recording index
	int         recordingjob_idx;       // recording job index
	int         track_idx;              // track index
#endif

#ifdef PROFILE_C_SUPPORT
	int         aceess_point_idx;       // access point info index
	int         door_idx;               // door info index
	int         area_idx;               // area info index
#endif

#ifdef PROFILE_G_SUPPORT
	int         search_idx;             // search index
#endif

#ifdef DEVICEIO_SUPPORT
	int         v_out_idx;              // video output index
	int         v_out_cfg_idx;          // video output configuration index
	int         a_out_idx;              // audio output index
	int         a_out_cfg_idx;          // audio output configuration index
	int         relay_idx;              // relay output index
	int         digit_input_idx;        // digit input index
	int         serial_port_idx;        // serial port index
#endif

#ifdef THERMAL_SUPPORT
	int         color_palette_idx;      // color palette index
	int         nuctable_idx;           // nuctable index
#endif

#ifdef CREDENTIAL_SUPPORT
	int         credential_idx;         // credential index
#endif

#ifdef ACCESS_RULES
	int         accessrule_idx;         // accessrule index
#endif

#ifdef SCHEDULE_SUPPORT
	int         schedule_idx;           // schedule index
	int         specialdaygroup_idx;    // special day group index
#endif

#ifdef RECEIVER_SUPPORT
	int         receiver_idx;           // receiver index
#endif

#ifdef VIDEO_ANALYTICS
	int         va_idx;                 // video analytics index
#endif

#ifdef STORAGE_SUPPORT
	int         storage_idx;            // storage index
#endif
}ONVIF_IDX;

typedef struct onvif_internal_msg
{
	char *      msg_dua;		        // message destination unit
	uint32	    msg_evt;		        // event / command value
	uint32	    msg_src;		        // message type
	int	        msg_len;		        // message buffer length
	char *      msg_buf;		        // message buffer
} OIMSG;


#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************/
#include "para.h"

extern ONVIF_CFG g_onvif_cfg;
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_IDX g_onvif_idx;

/***************************************************************************************/

HT_API char *                                    onvif_get_service_ip(uint32 lip, uint32 rip, char * sip, int len);
HT_API char *                                    onvif_get_service_ip_by_user(HTTPCLN * p_user, char * sip, int len);
HT_API char *                                    onvif_get_service_addr(onvif_CapabilityCategory type, uint32 lip, uint32 rip, char * saddr, int len);
HT_API char *                                    onvif_get_service_addr_by_user(onvif_CapabilityCategory type, HTTPCLN * p_user, char * saddr, int len);

/***************************************************************************************/
HT_API BOOL                                      onvif_is_scope_exist(const char * scope);
HT_API ONVIF_RET                                 onvif_add_scope(const char * scope, BOOL fixed);
HT_API onvif_Scope *                             onvif_find_scope(const char * scope);
HT_API onvif_Scope *                             onvif_get_idle_scope();
HT_API int                                       onvif_compare_nofind_scope(onvif_Scope * p_item, onvif_ScopeParseCnt strScopeCnt);

/***************************************************************************************/
HT_API BOOL                                      onvif_is_user_exist(const char * username);
HT_API void                                      onvif_refresh_User();
HT_API ONVIF_RET                                 onvif_add_user(onvif_User * p_user);
HT_API onvif_User *                              onvif_find_user(const char * username);
HT_API onvif_User *                              onvif_get_idle_user();
HT_API const char *                              onvif_get_user_pass(const char * username);

/***************************************************************************************/
HT_API LocationEntityList *                      onvif_add_LocationEntity(LocationEntityList ** p_head);
HT_API LocationEntityList *                      onvif_find_LocationEntity(LocationEntityList * p_head, const char * Entity, const char * Token);
HT_API void                                      onvif_free_LocationEntity(LocationEntityList ** p_head, LocationEntityList * p_node);
HT_API void                                      onvif_free_LocationEntitis(LocationEntityList ** p_head);
HT_API int                                       onvif_get_LocationEntity_nums(LocationEntityList * p_head);

HT_API void                                      onvif_get_StorageConfiguration_Token(StorageConfigurationList * p_head, char * token, int size);
HT_API StorageConfigurationList *                onvif_add_StorageConfiguration(StorageConfigurationList ** p_head);
HT_API StorageConfigurationList *                onvif_find_StorageConfiguration(StorageConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_StorageConfiguration(StorageConfigurationList ** p_head, StorageConfigurationList * p_node);
HT_API void                                      onvif_free_StorageConfigurations(StorageConfigurationList ** p_head);

HT_API void                                      onvif_get_profile_token(ONVIF_PROFILE * p_head, char * token, int size);
HT_API ONVIF_PROFILE *                           onvif_add_profile(ONVIF_PROFILE ** p_head, BOOL fixed);
HT_API ONVIF_PROFILE *                           onvif_find_profile(ONVIF_PROFILE * p_head, const char * token);
HT_API void                                      onvif_free_profiles(ONVIF_PROFILE ** p_head);

HT_API void                                      onvif_get_VideoSource_token(VideoSourceList * p_head, char * token, int size);
HT_API VideoSourceList *                         onvif_add_VideoSource(VideoSourceList ** p_head, int w, int h, int frame, int max_w, int max_h, int max_frame);
HT_API VideoSourceList *                         onvif_find_VideoSource(VideoSourceList * p_head, const char * token);
HT_API VideoSourceList *                         onvif_find_VideoSource_by_size(VideoSourceList * p_head, int w, int h);
HT_API void                                      onvif_free_VideoSources(VideoSourceList ** p_head);

HT_API void                                      onvif_get_VideoSourceConfiguration_token(VideoSourceConfigurationList * p_head, char * token, int size);
HT_API VideoSourceConfigurationList *            onvif_add_VideoSourceConfiguration(VideoSourceConfigurationList ** p_head, int w, int h);
HT_API VideoSourceConfigurationList *            onvif_find_VideoSourceConfiguration(VideoSourceConfigurationList * p_head, const char * token);
HT_API VideoSourceConfigurationList *            onvif_find_VideoSourceConfiguration_by_size(VideoSourceConfigurationList * p_head, int w, int h);
HT_API void                                      onvif_free_VideoSourceConfigurations(VideoSourceConfigurationList ** p_head);
HT_API void                                      onvif_init_VideoSourceConfigurationOptions(VideoSourceConfigurationList * p_item);

HT_API void                                      onvif_get_VideoEncoder2Configuration_token(VideoEncoder2ConfigurationList * p_head, char * token, int size);
HT_API VideoEncoder2ConfigurationList *          onvif_add_VideoEncoder2Configuration(VideoEncoder2ConfigurationList ** p_head, VideoEncoder2ConfigurationList * p_node);
HT_API VideoEncoder2ConfigurationList *          onvif_find_VideoEncoder2Configuration(VideoEncoder2ConfigurationList * p_head, const char * token);
HT_API VideoEncoder2ConfigurationList *          onvif_find_VideoEncoder2Configuration_by_param(VideoEncoder2ConfigurationList * p_head, VideoEncoder2ConfigurationList * p_node);
HT_API void                                      onvif_free_VideoEncoder2Configurations(VideoEncoder2ConfigurationList ** p_head);
HT_API void                                      onvif_init_VideoEncoderConfigurationOptions(VideoEncoder2ConfigurationList * p_item);

HT_API void                                      onvif_get_NetworkInterface_token(NetworkInterfaceList * p_head, char * token, int size);
HT_API NetworkInterfaceList *                    onvif_add_NetworkInterface(NetworkInterfaceList ** p_head);
HT_API NetworkInterfaceList *                    onvif_find_NetworkInterface(NetworkInterfaceList * p_head, const char * token);
HT_API void                                      onvif_free_NetworkInterfaces(NetworkInterfaceList ** p_head);

HT_API void onvif_renew_OSDConfigurations(OSDConfigurationList         *OSDs);

HT_API void                                      onvif_get_OSDConfiguration_token(OSDConfigurationList * p_head, char * token, int size);
HT_API OSDConfigurationList *                    onvif_add_OSDConfiguration(OSDConfigurationList ** p_head);
HT_API OSDConfigurationList *                    onvif_find_OSDConfiguration(OSDConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_OSDConfigurations(OSDConfigurationList ** p_head);

HT_API void                                      onvif_get_MetadataConfiguration_token(MetadataConfigurationList * p_head, char * token, int size);
HT_API MetadataConfigurationList *               onvif_add_MetadataConfiguration(MetadataConfigurationList ** p_head);
HT_API MetadataConfigurationList *               onvif_find_MetadataConfiguration(MetadataConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_MetadataConfigurations(MetadataConfigurationList ** p_head);

HT_API VideoEncoder2ConfigurationOptionsList *   onvif_add_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head);
HT_API VideoEncoder2ConfigurationOptionsList *   onvif_find_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList * p_head, const char * encoding);
HT_API void                                      onvif_free_VideoEncoder2ConfigurationOptions(VideoEncoder2ConfigurationOptionsList ** p_head);

/***************************************************************************************/
HT_API NotificationMessageList *                 onvif_add_NotificationMessage(NotificationMessageList ** p_head);
HT_API void                                      onvif_free_NotificationMessage(NotificationMessageList * p_message);
HT_API void                                      onvif_free_NotificationMessages(NotificationMessageList ** p_head);

HT_API SimpleItemList *                          onvif_add_SimpleItem(SimpleItemList ** p_head);
HT_API void                                      onvif_free_SimpleItems(SimpleItemList ** p_head);

HT_API ElementItemList *                         onvif_add_ElementItem(ElementItemList ** p_head);
HT_API void                                      onvif_free_ElementItems(ElementItemList ** p_head);

/***************************************************************************************/

#ifdef IMAGE_SUPPORT
HT_API void                                      onvif_get_ImagingPreset_token(ImagingPresetList * p_head, char * token, int size);
HT_API ImagingPresetList *                       onvif_add_ImagingPreset(ImagingPresetList ** p_head);
HT_API ImagingPresetList *                       onvif_find_ImagingPreset(ImagingPresetList * p_head, const char * token);
HT_API void                                      onvif_free_ImagingPresets(ImagingPresetList ** p_head);
#endif

/***************************************************************************************/

#ifdef AUDIO_SUPPORT
HT_API void                                      onvif_get_AudioSource_token(AudioSourceList * p_head, char * token, int size);
HT_API AudioSourceList *                         onvif_add_AudioSource(AudioSourceList ** p_head);
HT_API AudioSourceList *                         onvif_find_AudioSource(AudioSourceList * p_head, const char * token);
HT_API void                                      onvif_free_AudioSources(AudioSourceList ** p_head);

HT_API void                                      onvif_get_AudioSourceConfiguration_token(AudioSourceConfigurationList * p_head, char * token, int size);
HT_API AudioSourceConfigurationList *            onvif_add_AudioSourceConfiguration(AudioSourceConfigurationList ** p_head);
HT_API AudioSourceConfigurationList *            onvif_find_AudioSourceConfiguration(AudioSourceConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioSourceConfigurations(AudioSourceConfigurationList ** p_head);

HT_API void                                      onvif_get_AudioEncoder2Configuration_token(AudioEncoder2ConfigurationList * p_head, char * token, int size);
HT_API AudioEncoder2ConfigurationList *          onvif_add_AudioEncoder2Configuration(AudioEncoder2ConfigurationList ** p_head, AudioEncoder2ConfigurationList * p_node);
HT_API AudioEncoder2ConfigurationList *          onvif_find_AudioEncoder2Configuration(AudioEncoder2ConfigurationList * p_head, const char * token);
HT_API AudioEncoder2ConfigurationList *          onvif_find_AudioEncoder2Configuration_by_param(AudioEncoder2ConfigurationList * p_head, AudioEncoder2ConfigurationList * p_node);
HT_API void                                      onvif_free_AudioEncoder2Configurations(AudioEncoder2ConfigurationList ** p_head);

HT_API AudioEncoder2ConfigurationOptionsList *   onvif_add_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head);
HT_API AudioEncoder2ConfigurationOptionsList *   onvif_find_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList * p_head, const char * encoding);
HT_API void                                      onvif_free_AudioEncoder2ConfigurationOptions(AudioEncoder2ConfigurationOptionsList ** p_head);
HT_API void                                      onvif_init_AudioEncoderConfigurationOptions(AudioEncoder2ConfigurationList * p_item);

HT_API void                                      onvif_get_AudioDecoderConfiguration_token(AudioDecoderConfigurationList * p_head, char * token, int size);
HT_API AudioDecoderConfigurationList *           onvif_add_AudioDecoderConfiguration(AudioDecoderConfigurationList ** p_head);
HT_API AudioDecoderConfigurationList *           onvif_find_AudioDecoderConfiguration(AudioDecoderConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioDecoderConfigurations(AudioDecoderConfigurationList ** p_head);

HT_API void onvif_renew_AudioEncoderConfiguration();

#endif

/***************************************************************************************/

#ifdef MEDIA2_SUPPORT
HT_API void                                      onvif_get_Mask_token(MaskList * p_head, char * token, int size, int index);
HT_API void                                      onvif_del_Mask_token(int index);
HT_API MaskList *                                onvif_add_Mask(MaskList ** p_head, int index);
HT_API MaskList *                                onvif_find_Mask(MaskList * p_head, const char * token);
HT_API void                                      onvif_free_Masks(MaskList ** p_head);
HT_API void                                      onvif_renew_Masks();//MaskList      * mask)

#endif

/***************************************************************************************/

#ifdef PTZ_SUPPORT
HT_API void                                      onvif_get_PTZNode_token(PTZNodeList * p_head, char * token, int size);
HT_API PTZNodeList *                             onvif_add_PTZNode(PTZNodeList ** p_head);
HT_API PTZNodeList *                             onvif_find_PTZNode(PTZNodeList * p_head, const char * token);
HT_API void                                      onvif_free_PTZNodes(PTZNodeList ** p_head);

HT_API void                                      onvif_get_PTZConfiguration_token(PTZConfigurationList * p_head, char * token, int size);
HT_API PTZConfigurationList *                    onvif_add_PTZConfiguration(PTZConfigurationList ** p_head);
HT_API PTZConfigurationList *                    onvif_find_PTZConfiguration(PTZConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_PTZConfigurations(PTZConfigurationList ** p_head);
HT_API void                                      onvif_init_PTZConfigurationOptions(PTZConfigurationList * p_item);

HT_API void                                      onvif_get_PTZPreset_token(PTZPresetList * p_head, char * token, int size);
HT_API PTZPresetList *                           onvif_add_PTZPreset(PTZPresetList ** p_head);
HT_API PTZPresetList *                           onvif_find_PTZPreset(PTZPresetList * p_head, const char  * preset_token);
HT_API void                                      onvif_free_PTZPreset(PTZPresetList ** p_head, PTZPresetList * p_node);
HT_API void                                      onvif_free_PTZPresets(PTZPresetList ** p_head);

HT_API PTZPresetTourSpotList *                   onvif_add_PTZPresetTourSpot(PTZPresetTourSpotList ** p_head);
HT_API void                                      onvif_free_PTZPresetTourSpots(PTZPresetTourSpotList ** p_head);

HT_API void                                      onvif_get_PresetTour_token(PresetTourList * p_head, char * token, int size);
HT_API PresetTourList *                          onvif_add_PresetTour(PresetTourList ** p_head);
HT_API PresetTourList *                          onvif_find_PresetTour(PresetTourList * p_head, const char * token);
HT_API void                                      onvif_free_PresetTour(PresetTourList ** p_head, PresetTourList * p_node);
HT_API void                                      onvif_free_PresetTours(PresetTourList ** p_head);
HT_API int                                       onvif_count_PresetTours(PresetTourList * p_head);
#endif

/***************************************************************************************/

#ifdef VIDEO_ANALYTICS
HT_API ConfigList *                              onvif_add_Config(ConfigList ** p_head);
HT_API void                                      onvif_free_Config(ConfigList * p_node);
HT_API void                                      onvif_free_Configs(ConfigList ** p_head);
HT_API ConfigList *                              onvif_find_Config(ConfigList * p_head, const char * name);
HT_API ConfigList *                              onvif_find_Config_by_type(ConfigList * p_head, const char * type);
HT_API void                                      onvif_remove_Config(ConfigList ** p_head, ConfigList * p_remove);
HT_API ConfigList *                              onvif_get_prev_Config(ConfigList * p_head, ConfigList * p_found);

HT_API ConfigDescriptionList *                   onvif_add_ConfigDescription(ConfigDescriptionList ** p_head);
HT_API void                                      onvif_free_ConfigDescriptions(ConfigDescriptionList ** p_head);

HT_API ConfigDescription_MessagesList *          onvif_add_ConfigDescription_Message(ConfigDescription_MessagesList ** p_head);
HT_API void                                      onvif_free_ConfigDescription_Message(ConfigDescription_MessagesList * p_item);
HT_API void                                      onvif_free_ConfigDescription_Messages(ConfigDescription_MessagesList ** p_head);

HT_API ConfigOptionsList *                       onvif_add_ConfigOptions(ConfigOptionsList ** p_head);
HT_API void                                      onvif_free_ConfigOptions(ConfigOptionsList ** p_head);

HT_API SimpleItemDescriptionList *               onvif_add_SimpleItemDescription(SimpleItemDescriptionList ** p_head);
HT_API void                                      onvif_free_SimpleItemDescriptions(SimpleItemDescriptionList ** p_head);

HT_API void                                      onvif_get_VideoAnalyticsConfiguration_token(VideoAnalyticsConfigurationList * p_head, char * token, int size);
HT_API VideoAnalyticsConfigurationList *         onvif_add_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList ** p_head);
HT_API VideoAnalyticsConfigurationList *         onvif_find_VideoAnalyticsConfiguration(VideoAnalyticsConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_VideoAnalyticsConfigurations(VideoAnalyticsConfigurationList ** p_head);

void onvif_renew_VideoAnalyticsConfiguration();

void onvif_init_VideoAnalyticsConfiguration();

#endif

/***************************************************************************************/

#ifdef PROFILE_G_SUPPORT
HT_API void                                      onvif_get_Recording_token(RecordingList * p_head, char * token, int size);
HT_API RecordingList *                           onvif_add_Recording(RecordingList ** p_head);
HT_API RecordingList *                           onvif_find_Recording(RecordingList * p_head, const char * token);
HT_API void                                      onvif_free_Recording(RecordingList ** p_head, RecordingList * p_node);
HT_API void                                      onvif_free_Recordings(RecordingList ** p_head);

HT_API void                                      onvif_get_Track_token(TrackList * p_head, char * token, int size);
HT_API TrackList *                               onvif_add_Track(TrackList ** p_head);
HT_API void                                      onvif_free_Track(TrackList ** p_head, TrackList * p_track);
HT_API void                                      onvif_free_Tracks(TrackList ** p_head);
HT_API TrackList *                               onvif_find_Track(TrackList * p_head, const char * token);
HT_API int	                                     onvif_get_track_nums_by_type(TrackList * p_head, onvif_TrackType type);

HT_API void                                      onvif_get_RecordingJob_token(RecordingJobList * p_head, char * token, int size);
HT_API RecordingJobList *                        onvif_add_RecordingJob(RecordingJobList ** p_head);
HT_API RecordingJobList *                        onvif_find_RecordingJob(RecordingJobList * p_head, const char * token);
HT_API void                                      onvif_free_RecordingJob(RecordingJobList ** p_head, RecordingJobList * p_node);
HT_API void                                      onvif_free_RecordingJobs(RecordingJobList ** p_head);

HT_API RecordingInformationList *                onvif_add_RecordingInformation(RecordingInformationList ** p_head);
HT_API void                                      onvif_free_RecordingInformations(RecordingInformationList ** p_head);

HT_API FindEventResultList *                     onvif_add_FindEventResult(FindEventResultList ** p_head);
HT_API void                                      onvif_free_FindEventResult(FindEventResultList ** p_head, FindEventResultList * p_node);
HT_API void                                      onvif_free_FindEventResults(FindEventResultList ** p_head);

HT_API FindMetadataResultList *                  onvif_add_FindMetadataResult(FindMetadataResultList ** p_head);
HT_API void                                      onvif_free_FindMetadataResult(FindMetadataResultList ** p_head, FindMetadataResultList * p_node);
HT_API void                                      onvif_free_FindMetadataResults(FindMetadataResultList ** p_head);

HT_API FindPTZPositionResultList *               onvif_add_FindPTZPositionResult(FindPTZPositionResultList ** p_head);
HT_API void                                      onvif_free_FindPTZPositionResult(FindPTZPositionResultList ** p_head, FindPTZPositionResultList * p_node);
HT_API void                                      onvif_free_FindPTZPositionResults(FindPTZPositionResultList ** p_head);
#endif

/***************************************************************************************/

#ifdef PROFILE_C_SUPPORT
HT_API void                                      onvif_get_AccessPoint_token(AccessPointList * p_head, char * token, int size);
HT_API AccessPointList *                         onvif_add_AccessPoint(AccessPointList ** p_head);
HT_API AccessPointList *                         onvif_find_AccessPoint(AccessPointList * p_head, const char * token);
HT_API void                                      onvif_free_AccessPoint(AccessPointList ** p_head, AccessPointList * p_node);
HT_API void                                      onvif_free_AccessPoints(AccessPointList ** p_head);

HT_API DoorInfoList *                            onvif_add_DoorInfo(DoorInfoList ** p_head);
HT_API DoorInfoList *                            onvif_find_DoorInfo(DoorInfoList * p_head, const char * token);
HT_API void                                      onvif_free_DoorInfos(DoorInfoList ** p_head);

HT_API void                                      onvif_get_Door_token(DoorList * p_head, char * token, int size);
HT_API DoorList *                                onvif_add_Door(DoorList ** p_head);
HT_API DoorList *                                onvif_find_Door(DoorList * p_head, const char * token);
HT_API void                                      onvif_free_Door(DoorList ** p_head, DoorList * p_node);
HT_API void                                      onvif_free_Doors(DoorList ** p_head);

HT_API void                                      onvif_get_Area_token(AreaList * p_head, char * token, int size);
HT_API AreaList *                                onvif_add_Area(AreaList ** p_head);
HT_API AreaList *                                onvif_find_Area(AreaList * p_head, const char * token);
HT_API void                                      onvif_free_Area(AreaList ** p_head, AreaList * p_node);
HT_API void                                      onvif_free_Areas(AreaList ** p_head);
#endif // end of PROFILE_C_SUPPORT

/***************************************************************************************/

#ifdef DEVICEIO_SUPPORT
HT_API PaneLayoutList *                          onvif_add_PaneLayout(PaneLayoutList ** p_head);
HT_API PaneLayoutList *                          onvif_find_PaneLayout(PaneLayoutList * p_head, const char * token);
HT_API void                                      onvif_free_PaneLayouts(PaneLayoutList ** p_head);

HT_API void                                      onvif_get_VideoOutput_token(VideoOutputList * p_head, char * token, int size);
HT_API VideoOutputList *                         onvif_add_VideoOutput(VideoOutputList ** p_head);
HT_API VideoOutputList *                         onvif_find_VideoOutput(VideoOutputList * p_head, const char * token);
HT_API void                                      onvif_free_VideoOutputs(VideoOutputList ** p_head);

HT_API void                                      onvif_get_VideoOutputConfiguration_token(VideoOutputConfigurationList * p_head, char * token, int size);
HT_API VideoOutputConfigurationList *            onvif_add_VideoOutputConfiguration(VideoOutputConfigurationList ** p_head);
HT_API VideoOutputConfigurationList *            onvif_find_VideoOutputConfiguration(VideoOutputConfigurationList * p_head, const char * token);
HT_API VideoOutputConfigurationList *            onvif_find_VideoOutputConfiguration_by_OutputToken(VideoOutputConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_VideoOutputConfigurations(VideoOutputConfigurationList ** p_head);

HT_API void                                      onvif_get_AudioOutput_token(AudioOutputList * p_head, char * token, int size);
HT_API AudioOutputList *                         onvif_add_AudioOutput(AudioOutputList ** p_head);
HT_API AudioOutputList *                         onvif_find_AudioOutput(AudioOutputList * p_head, const char * token);
HT_API void                                      onvif_free_AudioOutputs(AudioOutputList ** p_head);

HT_API void                                      onvif_get_AudioOutputConfiguration_token(AudioOutputConfigurationList * p_head, char * token, int size);
HT_API AudioOutputConfigurationList *            onvif_add_AudioOutputConfiguration(AudioOutputConfigurationList ** p_head);
HT_API AudioOutputConfigurationList *            onvif_find_AudioOutputConfiguration(AudioOutputConfigurationList * p_head, const char * token);
HT_API AudioOutputConfigurationList *            onvif_find_AudioOutputConfiguration_by_OutputToken(AudioOutputConfigurationList * p_head, const char * token);
HT_API void                                      onvif_free_AudioOutputConfigurations(AudioOutputConfigurationList ** p_head);

HT_API void                                      onvif_get_RelayOutput_token(RelayOutputList * p_head, char * token, int size);
HT_API RelayOutputList *                         onvif_add_RelayOutput(RelayOutputList ** p_head);
HT_API RelayOutputList *                         onvif_find_RelayOutput(RelayOutputList * p_head, const char * token);
HT_API void                                      onvif_free_RelayOutputs(RelayOutputList ** p_head);

HT_API void                                      onvif_get_DigitalInput_token(DigitalInputList * p_head, char * token, int size);
HT_API DigitalInputList *                        onvif_add_DigitalInput(DigitalInputList ** p_head);
HT_API DigitalInputList *                        onvif_find_DigitalInput(DigitalInputList * p_head, const char * token);
HT_API void                                      onvif_free_DigitalInputs(DigitalInputList ** p_head);

HT_API void                                      onvif_get_SerialPort_token(SerialPortList * p_head, char * token, int size);
HT_API SerialPortList *                          onvif_add_SerialPort(SerialPortList ** p_head);
HT_API SerialPortList *                          onvif_find_SerialPort(SerialPortList * p_head, const char * token);
HT_API SerialPortList *                          onvif_find_SerialPort_by_ConfigurationToken(SerialPortList * p_head, const char * token);
HT_API void                                      onvif_free_SerialPorts(SerialPortList ** p_head);
HT_API void                                      onvif_malloc_SerialData(onvif_SerialData * p_data, int union_SerialData, int size);
HT_API void                                      onvif_free_SerialData(onvif_SerialData * p_data);
#endif // end of DEVICEIO_SUPPORT

#ifdef IMAGE_SUPPORT
HT_API void                                     onvif_refresh_ImagingSettings(onvif_ImagingSettings * p_item);

HT_API void                                     onvif_init_ImagingSettings(VideoSourceList * p_vsrc, onvif_ImagingSettings * p_item);
HT_API void                                     onvif_init_ImagingOptions(VideoSourceList * p_vsrc, onvif_ImagingOptions * p_item);
#endif

/***************************************************************************************/

#ifdef THERMAL_SUPPORT
HT_API void                                      onvif_get_ColorPalette_token(ColorPaletteList * p_head, char * token, int size);
HT_API ColorPaletteList *                        onvif_add_ColorPalette(ColorPaletteList ** p_head);
HT_API ColorPaletteList *                        onvif_find_ColorPalette(ColorPaletteList * p_head, const char * token);
HT_API void                                      onvif_free_ColorPalettes(ColorPaletteList ** p_head);

HT_API void                                      onvif_get_NUCTable_token(NUCTableList * p_head, char * token, int size);
HT_API NUCTableList *                            onvif_add_NUCTable(NUCTableList ** p_head);
HT_API NUCTableList *                            onvif_find_NUCTable(NUCTableList * p_head, const char * token);
HT_API void                                      onvif_free_NUCTables(NUCTableList ** p_head);

HT_API BOOL                                      onvif_init_Thermal(VideoSourceList * p_req);
#endif // end of THERMAL_SUPPORT

/***************************************************************************************/

#ifdef CREDENTIAL_SUPPORT
HT_API void                                      onvif_get_Credential_token(CredentialList * p_head, char * token, int size);
HT_API CredentialList *                          onvif_add_Credential(CredentialList ** p_head);
HT_API CredentialList *                          onvif_find_Credential(CredentialList * p_head, const char * token);
HT_API void                                      onvif_free_Credential(CredentialList ** p_head, CredentialList * p_node);
HT_API void                                      onvif_free_Credentials(CredentialList ** p_head);
HT_API BOOL                                      onvif_init_Credential();

HT_API CredentialIdentifierItemList *            onvif_add_CredentialIdentifierItem(CredentialIdentifierItemList ** p_head);
HT_API void                                      onvif_free_CredentialIdentifierItem(CredentialIdentifierItemList ** p_head, CredentialIdentifierItemList * p_item);
HT_API void                                      onvif_free_CredentialIdentifierItems(CredentialIdentifierItemList ** p_head);
#endif // end of CREDENTIAL_SUPPORT

/***************************************************************************************/

#ifdef ACCESS_RULES
HT_API void                                      onvif_get_AccessProfile_token(AccessProfileList * p_head, char * token, int size);
HT_API AccessProfileList *                       onvif_add_AccessProfile(AccessProfileList ** p_head);
HT_API AccessProfileList *                       onvif_find_AccessProfile(AccessProfileList * p_head, const char * token);
HT_API void                                      onvif_free_AccessProfile(AccessProfileList ** p_head, AccessProfileList * p_node);
HT_API void                                      onvif_free_AccessProfiles(AccessProfileList ** p_head);
HT_API BOOL                                      onvif_init_AccessProfile();
#endif // end of ACCESS_RULES

/***************************************************************************************/

#ifdef SCHEDULE_SUPPORT
HT_API void                                      onvif_get_Schedule_token(ScheduleList * p_head, char * token, int size);
HT_API ScheduleList *                            onvif_add_Schedule(ScheduleList ** p_head);
HT_API ScheduleList *                            onvif_find_Schedule(ScheduleList * p_head, const char * token);
HT_API void                                      onvif_free_Schedule(ScheduleList ** p_head, ScheduleList * p_node);
HT_API void                                      onvif_free_Schedules(ScheduleList ** p_head);
HT_API BOOL                                      onvif_init_Schedule();

HT_API void                                      onvif_get_SpecialDayGroup_token(SpecialDayGroupList * p_head, char * token, int size);
HT_API SpecialDayGroupList *                     onvif_add_SpecialDayGroup(SpecialDayGroupList ** p_head);
HT_API SpecialDayGroupList *                     onvif_find_SpecialDayGroup(SpecialDayGroupList * p_head, const char * token);
HT_API void                                      onvif_free_SpecialDayGroup(SpecialDayGroupList ** p_head, SpecialDayGroupList * p_node);
HT_API void                                      onvif_free_SpecialDayGroups(SpecialDayGroupList ** p_head);
HT_API BOOL                                      onvif_init_SpecialDayGroup();
#endif // end of SCHEDULE_SUPPORT

/***************************************************************************************/

#ifdef RECEIVER_SUPPORT
HT_API void                                      onvif_get_Receiver_token(ReceiverList * p_head, char * token, int size);
HT_API ReceiverList *                            onvif_add_Receiver(ReceiverList ** p_head);
HT_API ReceiverList *                            onvif_find_Receiver(ReceiverList * p_head, const char * token);
HT_API void                                      onvif_free_Receiver(ReceiverList ** p_head, ReceiverList * p_node);
HT_API void                                      onvif_free_Receivers(ReceiverList ** p_head);
HT_API int                                       onvif_get_Receiver_nums(ReceiverList * p_head);
#endif // end of RECEIVER_SUPPORT

/***************************************************************************************/

#ifdef IPFILTER_SUPPORT
HT_API BOOL                                      onvif_is_ipaddr_filter_exist(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
HT_API onvif_PrefixedIPAddress *                 onvif_find_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
HT_API onvif_PrefixedIPAddress *                 onvif_get_idle_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size);
HT_API ONVIF_RET                                 onvif_add_ipaddr_filter(onvif_PrefixedIPAddress * p_head, int size, onvif_PrefixedIPAddress * p_item);
#endif // IPFILTER_SUPPORT

/***************************************************************************************/
HT_API void                                      onvif_init_MulticastConfiguration(onvif_MulticastConfiguration * p_cfg);
HT_API BOOL                                      onvif_init_ZeroConfiguration();
HT_API void                                      onvif_init_capabilities();
HT_API void                                      onvif_init_def_cfg();
HT_API void                                      onvif_init_cfg();
HT_API void                                      onvif_init();



#ifdef __cplusplus
}
#endif

#endif


