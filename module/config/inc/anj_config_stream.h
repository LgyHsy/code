#ifndef _ANJ_CONFIG_STREAM_H_
#define _ANJ_CONFIG_STREAM_H_

#include "ixml.h"
#include "anj_mw_time.h"

#ifdef __cplusplus
extern "C"
{
#endif
#define MAX_IP_NAME_LEN 64
#define MAX_IPC_FILENAME_LEN 128

typedef struct
{
    short enable;
    unsigned short Port;
    unsigned int Ip;
} MulticastStruct;

typedef struct
{
    int enable_onvif;
    int enable_web;
    // int enable_https;
    int onvif_auth;
    int webPort;
    int httpsPort;                               // HTTPS端口，需要能力集 FUNCTION_HTTPS
    int h5Port;                                  // H5播放端口，需要能力集 FUNCTION_H5
    char httpsCertificate[MAX_IPC_FILENAME_LEN]; // https证书文件，需要能力集 FUNCTION_HTTPS
    char httpsKey[MAX_IPC_FILENAME_LEN];         // https密钥文件，需要能力集 FUNCTION_HTTPS
} WebConfig;

typedef struct
{
    int enable;
    unsigned short port;
    short auth;
} HikConfig;

typedef struct
{
    int enable;
    unsigned short port;
    short auth;
} DhConfig;

#define MAX_RTMP_APP_NAME_LEN 64
#define MAX_RTMP_STREAMID_LEN 256

typedef struct
{
    int enable;
    char server[MAX_IP_NAME_LEN];
    unsigned short port;
    short streamno; // 0: main 1: sub 2: third
    char appname[MAX_RTMP_APP_NAME_LEN];
    char streamid[MAX_RTMP_STREAMID_LEN];
    int type;             // reserve
    TimeSpanCfg timeSpan; // 推流时间
} RtmpConfig;

typedef struct
{
    int enable_rtsp;
    int rtsp_auth;
    int rtpoverrtsp;
    int videoPort;
} RtspConfig;

typedef struct
{
    int enable;
    int ptzPort;
} CommConfig;

typedef struct
{
    int enable;
} TstConfig;

typedef struct
{
    char onvif_expand;
    char smart_nvr;
    char privatetype;
} UnvConfig;

typedef struct
{
    MulticastStruct StreamMulticast[2];
} MulticastConfig;

typedef struct
{
    RtspConfig rtspConfig;
    CommConfig commConfig;
    WebConfig webConfig;
    HikConfig hikConfig;
    DhConfig dhConfig;
    TstConfig tstConfig;
    UnvConfig unvConfig;
    RtmpConfig rtmpConfig;
    MulticastConfig multicastConfig;
} MediaStreamConfig;

int anj_config_stream_default(MediaStreamConfig *pMediaStreamCfg);

int anj_config_stream_get(IXML_Node *pNode, MediaStreamConfig *pMediaStreamCfg);

char *anj_config_stream_conver_xml(MediaStreamConfig *pMediaStreamCfg);

char *anj_config_stream_search_conver_xml(MediaStreamConfig *pMediaStreamCfg);

char *anj_config_stream_rtmp_conver_xml(RtmpConfig *pRtmpConfig);

int anj_config_stream_save(MediaStreamConfig *pMediaStreamCfg);

int anj_config_stream_set(MediaStreamConfig *pstStreamConfig);

int anj_config_stream_load(MediaStreamConfig *pstStreamConfig);

int anj_config_stream_get_by_xml(MediaStreamConfig *pMediaStream, char *xmlBuf, int bHaveOldCfg);

int anj_config_stream_port_check(MediaStreamConfig *pCfg);

#ifdef __cplusplus
}
#endif

#endif
