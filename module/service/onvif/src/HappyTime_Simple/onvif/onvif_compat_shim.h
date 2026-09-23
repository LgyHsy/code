/*
 * Compatibility shim for HappyTime_Simple ONVIF integration.
 *
 * The HappyTime_Simple vendor code was originally built against the ipc1
 * platform SDK (server/tools/system_msg.c, server/tools/aux_msg.c etc.).
 * This file bridges the gap to the ipc2 platform APIs (anj_config,
 * anj_sysmng, ...).
 */

#ifndef ONVIF_COMPAT_SHIM_H
#define ONVIF_COMPAT_SHIM_H

#include <pthread.h>
#include <time.h>
#include "anj_config.h"
#include "anj_config_media.h"
#include "anj_config_stream.h"
#include "anj_config_network.h"
#include "anj_config_alarm.h"
#include "anj_config_system.h"
#include "anj_config_oem.h"
#include "anj_sysmng.h"
#include "anj_mw_comm.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_mw_crypt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* Legacy product type enum (ipc1 compat)                              */
/* In ipc2, product_type_read() always returns PRODUCT_TYPE_BASE (0),  */
/* so all product-type branch conditions evaluate to false.            */
/* ================================================================== */
#ifndef PRODUCT_TYPE_BASE
enum {
    PRODUCT_TYPE_BASE = 0,
    PRODUCT_TYPE_AC400L = 0x1001,
    PRODUCT_TYPE_MC400L2,
    PRODUCT_TYPE_MS400L2,
    PRODUCT_TYPE_MCL15,
    PRODUCT_TYPE_MCF46,
    PRODUCT_TYPE_MCL16,
    PRODUCT_TYPE_MHC31J4,
    PRODUCT_TYPE_MT200E5,
    PRODUCT_TYPE_MY200E3,
    PRODUCT_TYPE_MTE6,
    PRODUCT_TYPE_MTE8,
    PRODUCT_TYPE_MTE9,
    PRODUCT_TYPE_MTJ21,
    PRODUCT_TYPE_MSF50W,
    PRODUCT_TYPE_XZA1,
    PRODUCT_TYPE_MYQ10,
    PRODUCT_TYPE_MYQ11,
    PRODUCT_TYPE_MYQ11B,
    PRODUCT_TYPE_MYQ12,
    PRODUCT_TYPE_MYQ12D,
    PRODUCT_TYPE_MYA5,
    PRODUCT_TYPE_MYA12,
    PRODUCT_TYPE_MYA20,
    PRODUCT_TYPE_MYA25,
    PRODUCT_TYPE_MYD10,
    PRODUCT_TYPE_MYD20W,
    PRODUCT_TYPE_MYD21W,
    PRODUCT_TYPE_MYE10,
    PRODUCT_TYPE_MYE10Q,
    PRODUCT_TYPE_MYE11,
    PRODUCT_TYPE_MYE13,
    PRODUCT_TYPE_MYE15,
    PRODUCT_TYPE_MYE20,
    PRODUCT_TYPE_MYE40,
    PRODUCT_TYPE_MYF5,
    PRODUCT_TYPE_MYV25,
    PRODUCT_TYPE_MYY10,
    PRODUCT_TYPE_MYY40,
    PRODUCT_TYPE_T30G,
};
#endif

/* Legacy AudioType enum (ipc1 compat) */
typedef enum _AudioType_e {
    AudioType_PCMU,
    AudioType_AAC_LC,
    AudioType_PCMA,
    AudioType_PCM,
    AudioType_OPUS,
    AudioType_MP3,
} AudioType_e;

/* ipc1 uses 'VideoCapture', ipc2 uses 'VideoCaptureCfg' */
typedef VideoCaptureCfg VideoCapture;

/* Legacy message queue keys (ipc1 compat) */
#ifndef WEB_PROCESS_KEY
#define WEB_PROCESS_KEY 0x10
#endif

/* Global variables */
extern WIFIConfig g_wifiCfg;
extern int g_stereo;
extern unsigned int g_motion_mode;
extern int g_openeye_smw;
extern char g_ifname[256];
extern pthread_mutex_t g_mclock;
extern int g_yen_version_sdm;
extern int g_yen_version_smd;
extern int g_yen_version_ultramotion_clock;

/* Msg* config get helpers (old IPC-queue API -> new anj_config) */
int MsgGetMediaConfig(MediaConfig *pMediaCfg);
int MsgGetMediaStreamConfig(MediaStreamConfig *pMediaStreamCfg);
int MsgGetNetworkLANConfig(LANConfig *lanCfg);
int MsgGetUserConfig(UserConfig *pUserCfg);
int MsgGetSystemVersionInfo(SYSTEM_VERSION_DATA *versionInfo);
int MsgGetVideoEncodeConfig(VideoEncode *pCfg);
int MsgGetVideoCaptureConfig(VideoCapture *pCfg);

/* Msg* config set helpers */
int MsgSetVideoEncodeConfig(void *p);
int MsgSetIp(const char *ip_addr, const char *netmask, const char *gateway);
int MsgEnableDhcp(void);
int MsgConfigNetwork(void);

/* Misc utility functions */
int GetVideoResArray(RESOLUTION_ENTRY **pEntry);
int ReadEncriptData(void *buf, int len);
int AuxMsgPTZCmd(const char *xml_cmd);
int product_type_read(void);
const char *net_get_wirelessap_name(void);

/* ipc1 GetOnvifOemInfo -> ipc2 anj_config_oem_onvif_get */
#define GetOnvifOemInfo(p) anj_config_oem_onvif_get(p)

/* HTTP_LOG macro from ipc1 cgi_def.h */
#ifndef HTTP_LOG
#define HTTP_LOG __INFO
#endif

/* Audio encoder type lookup from ipc1 media_inf.h */
int GetAudioEncoderType(const char *decoderName);

#ifdef __cplusplus
}
#endif

#endif /* ONVIF_COMPAT_SHIM_H */
