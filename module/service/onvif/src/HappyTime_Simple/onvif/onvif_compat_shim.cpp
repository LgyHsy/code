/*
 * Compatibility shim for HappyTime_Simple ONVIF integration.
 *
 * The HappyTime_Simple vendor code was originally built against the ipc1
 * platform SDK (server/tools/system_msg.c, server/tools/aux_msg.c etc.).
 * This file bridges the gap to the ipc2 platform APIs (anj_config,
 * anj_sysmng, ...).
 *
 * Symbols marked __attribute__((weak)) are silently overridden when a
 * strong definition exists elsewhere in the link.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

extern "C" {

#include "anj_config.h"
#include "anj_config_media.h"
#include "anj_config_stream.h"
#include "anj_config_network.h"
#include "anj_config_alarm.h"
#include "anj_config_system.h"
#include "anj_config_oem.h"
#include "anj_sysmng.h"
#include "anj_mw_comm.h"

}

#include "onvif_compat_shim.h"

#ifndef WEAK_SYMBOL
#define WEAK_SYMBOL __attribute__((weak))
#endif

/* ================================================================== */
/*  Global variables                                                   */
/* ================================================================== */

extern "C" {

WEAK_SYMBOL WIFIConfig g_wifiCfg;
WEAK_SYMBOL int g_stereo = 0;
WEAK_SYMBOL unsigned int g_motion_mode = 0;
WEAK_SYMBOL int g_openeye_smw = 0;
WEAK_SYMBOL char g_ifname[256] = {0};
WEAK_SYMBOL pthread_mutex_t g_mclock = PTHREAD_MUTEX_INITIALIZER;
WEAK_SYMBOL int g_yen_version_sdm = 0;
WEAK_SYMBOL int g_yen_version_smd = 0;
WEAK_SYMBOL int g_yen_version_ultramotion_clock = 0;

/* ================================================================== */
/*  Msg* -- config get helpers (old IPC-queue API -> new anj_config)    */
/* ================================================================== */

WEAK_SYMBOL int MsgGetMediaConfig(MediaConfig *pMediaCfg)
{
    if (!pMediaCfg) return -1;
    void *src = getMediaConfig();
    if (src) {
        memcpy(pMediaCfg, src, sizeof(MediaConfig));
        return 0;
    }
    return -1;
}

WEAK_SYMBOL int MsgGetMediaStreamConfig(MediaStreamConfig *pMediaStreamCfg)
{
    if (!pMediaStreamCfg) return -1;
    void *src = getMediaStreamConfig();
    if (src) {
        memcpy(pMediaStreamCfg, src, sizeof(MediaStreamConfig));
        return 0;
    }
    return -1;
}

WEAK_SYMBOL int MsgGetNetworkLANConfig(LANConfig *lanCfg)
{
    if (!lanCfg) return -1;
    void *src = getNetWorkConfig();
    if (src) {
        memcpy(lanCfg, src, sizeof(LANConfig));
        return 0;
    }
    return -1;
}

WEAK_SYMBOL int MsgGetUserConfig(UserConfig *pUserCfg)
{
    if (!pUserCfg) return -1;
    void *src = getSystemConfig();
    if (src) {
        SystemConfig *sysCfg = (SystemConfig *)src;
        memcpy(pUserCfg, &sysCfg->userCfg, sizeof(UserConfig));
        return 0;
    }
    return -1;
}

WEAK_SYMBOL int MsgGetSystemVersionInfo(SYSTEM_VERSION_DATA *versionInfo)
{
    if (!versionInfo) return -1;
    return anj_sysmng_version_info_get(versionInfo, 0);
}

WEAK_SYMBOL int MsgGetVideoEncodeConfig(VideoEncode *pCfg)
{
    if (!pCfg) return -1;
    MediaConfig *mc = (MediaConfig *)getMediaConfig();
    if (mc) {
        memcpy(pCfg, &mc->videoConfig[0].videoEncode, sizeof(VideoEncode));
        return 0;
    }
    return -1;
}

WEAK_SYMBOL int MsgGetVideoCaptureConfig(VideoCaptureCfg *pCfg)
{
    if (!pCfg) return -1;
    MediaConfig *mc = (MediaConfig *)getMediaConfig();
    if (mc) {
        memcpy(pCfg, &mc->videoConfig[0].videoCapture, sizeof(VideoCaptureCfg));
        return 0;
    }
    return -1;
}

/* ================================================================== */
/*  Msg* -- config set helpers                                         */
/* ================================================================== */

WEAK_SYMBOL int MsgSetVideoEncodeConfig(void *p) { (void)p; return 0; }

WEAK_SYMBOL int MsgSetIp(const char *ip_addr, const char *netmask, const char *gateway)
{
    (void)ip_addr;
    (void)netmask;
    (void)gateway;
    return 0;
}

WEAK_SYMBOL int MsgEnableDhcp(void) { return 0; }
WEAK_SYMBOL int MsgConfigNetwork(void) { return 0; }

/* ================================================================== */
/*  Misc utility functions                                             */
/* ================================================================== */

WEAK_SYMBOL int GetVideoResArray(RESOLUTION_ENTRY **pEntry)
{
    return anj_sysmng_video_res_array_get(pEntry);
}

WEAK_SYMBOL int ReadEncriptData(void *buf, int len)
{
    if (buf != NULL && len > 0) {
        memset(buf, 0, (size_t)len);
    }
    return 0;
}

WEAK_SYMBOL int AuxMsgPTZCmd(const char *xml_cmd)
{
    (void)xml_cmd;
    return 0;
}

WEAK_SYMBOL int product_type_read(void)
{
    return 0; /* PRODUCT_TYPE_BASE */
}

WEAK_SYMBOL const char *net_get_wirelessap_name(void)
{
    return "wlan0";
}

WEAK_SYMBOL int GetAudioEncoderType(const char *decoderName)
{
    if (!decoderName) return -1;
    if (strcasecmp(decoderName, "PCMA") == 0
        || strcasecmp(decoderName, "G711A") == 0
        || strcasecmp(decoderName, "G.711A") == 0)
        return AudioType_PCMA;
    if (strcasecmp(decoderName, "PCMU") == 0
        || strcasecmp(decoderName, "G711") == 0
        || strcasecmp(decoderName, "G.711") == 0
        || strcasecmp(decoderName, "G711U") == 0
        || strcasecmp(decoderName, "G.711U") == 0)
        return AudioType_PCMU;
    if (strcasecmp(decoderName, "PCM") == 0)
        return AudioType_PCM;
    if (strcasecmp(decoderName, "MPEG4-GENERIC") == 0
        || strcasecmp(decoderName, "AAC") == 0
        || strcasecmp(decoderName, "MP4A") == 0)
        return AudioType_AAC_LC;
    return -1;
}

} /* extern "C" */
