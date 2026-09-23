#ifndef _ANJ_MW_MEDIA_COMMON_H_
#define _ANJ_MW_MEDIA_COMMON_H_

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "anj_mw_comm.h"
#include "anj_mw_list.h"

/* TS MPP SDK (ST/sdk_include): comm 类型在前，MPI 接口在后 */
#include "ts_type.h"
#include "ts_common.h"
#include "ts_errno.h"
// #include "ts_buffer.h"
#include "ts_comm_vb.h"
#include "ts_comm_video.h"
#include "ts_comm_sys.h"
#include "ts_comm_vi.h"
#include "ts_comm_vpss.h"
#include "ts_comm_venc.h"
#include "ts_comm_isp.h"
#include "ts_comm_region.h"
#include "ts_comm_aio.h"
#include "ts_comm_aenc.h"
#include "ts_comm_adec.h"

#include "mpi_sys.h"
#include "mpi_log.h"
#include "mpi_vb.h"
#include "mpi_vi.h"
#include "mpi_vpss.h"
#include "mpi_venc.h"
#include "mpi_isp.h"
#include "mpi_region.h"
#include "mpi_aiisp.h"
#include "mpi_audio.h"


#include "anj_mw_media_audio.h"

#define DEFAULT_SENSOR_FPS (25)
/* OS05A20: 2lane 2592x1944 stagger 仅 15fps；30fps stagger 需 4lane */
#define WDR_SENSOR_FPS (25)
#define TS_AUDIO_PLAY_PERIOD_SIZE    (1024)
#define AIISP_MAX_FPS      (12)
#define TS_VENC_BUFSIZE_MIN (192 * 1024)
/* ST semantic alias: VPSS channel count */
#define MAX_VPSS_CHN MAX_SCL_PORT

#ifndef ExecFunc
#define ExecFunc(_func_, _ret_)                                \
do                                                         \
{                                                          \
    TS_S32 s32Ret = TS_SUCCESS;                            \
    s32Ret = _func_;                                       \
    if (s32Ret != _ret_)                                   \
    {                                                      \
        __ERR("exec function failed, error=0x%x\n", (unsigned int)s32Ret); \
        return s32Ret;                                     \
    }                                                      \
    else                                                   \
    {                                                      \
        __INFO("exec function pass\n");                    \
    }                                                      \
} while (0)
#endif

#ifndef STCHECKRESULT
#define STCHECKRESULT(_func_)                                  \
do                                                         \
{                                                          \
    TS_S32 s32Ret = TS_SUCCESS;                            \
    s32Ret = _func_;                                       \
    if (s32Ret != TS_SUCCESS)                              \
    {                                                      \
        __ERR("exec function failed, error=0x%x\n", (unsigned int)s32Ret); \
        return s32Ret;                                     \
    }                                                      \
} while (0)
#endif

#define CHECK_PARAM_IS_X(PARAM, X, RET, errinfo)                   \
do                                                             \
{                                                              \
    if ((PARAM) == (X))                                        \
    {                                                          \
        __ERR("The input Mixer Rgn Param pointer is NULL!\n"); \
        return RET;                                            \
    }                                                          \
} while (0);

#define CHECK_PARAM_OPT_X(PARAM, OPT, X, RET, errinfo) \
do                                                 \
{                                                  \
    if ((PARAM)OPT(X))                             \
    {                                              \
        __ERR("%s\n", errinfo);                    \
        return RET;                                \
    }                                              \
} while (0);

#ifndef RGB2PIXEL1555
#define RGB2PIXEL1555(a, r, g, b) (((a & 0x80) << 8) | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | ((b & 0xF8) >> 3))
#endif

#define TS_FAILED (-1)

#define MAX_VDF_NUM_PER_CHN 16
#define NALU_PACKET_SIZE 512 * 1024

typedef struct
{
    TS_U32 u32X;
    TS_U32 u32Y;
} TS_Point_T;

typedef void (* TS_Common_Vpss_DataCb)(int u32DevId, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param);

typedef struct TS_Common_VpssAttr_s
{
    TS_BOOL bEnable;
    VPSS_GRP VpssGrpId;
    VPSS_CHN VpssChnId;
    TS_U32 u32Width;
    TS_U32 u32Height;
    TS_U32 u32SrcFrmRateNum;
    TS_BOOL bMirror;
    TS_BOOL bFlip;
    TS_U32 u32DepthSet;
    TS_U32 u32MemCnt;
    TS_Common_Vpss_DataCb datacb;
    int isjpeg; /* tx：抓拍走 VENC，此处恒 0；保留字段与其它平台/调试日志对齐 */
    void *param;
} TS_Common_VpssAttr_t;

typedef void (* TS_Common_Venc_DataCb)(int VencChn, int iskey, char *data, int len, unsigned int u32Seq, int codec, unsigned long long int timestamp);

typedef struct tsTS_Common_ViPipe_S {
    VI_PIPE aPipe;
    VI_VPSS_MODE_E enMastPipeMode;
    PIXEL_FORMAT_E enPixFmt;
    DATA_BITWIDTH_E enBitWid;
    TS_U8 u8PixCut;
    TS_U8 u8RowCut;
    ISP_BAYER_FORMAT_E enBayer;
    WDR_MODE_E enWdrMode;
    TS_BOOL bIspByFly;
    TS_BOOL bDynFpsSync;
    TS_U32 width;
    TS_U32 height;
    TS_FLOAT frameRate;
    TS_BOOL bHFlip;
    TS_BOOL bVFlip;
} TS_Common_ViPipe_t;

typedef struct tsTS_Common_ViChn_S {
    VI_CHN ViChn[VI_MAX_CHN_NUM];
    TS_U32 validChnlNum;
    PIXEL_FORMAT_E enPixFormat;
    TS_U32 width[VI_MAX_CHN_NUM];
    TS_U32 height[VI_MAX_CHN_NUM];
} TS_Common_ViChn_t;

typedef struct TS_Common_ViInfo_s
{
    VI_PIPE ViPipe;
    VI_CHN ViChn;
    TS_Common_ViPipe_t stPipeInfo;
    TS_Common_ViChn_t stChnInfo;
} TS_Common_ViInfo_t;

typedef struct TS_Common_ViAttr_s
{
    TS_Common_ViInfo_t astViInfo[VI_MAX_PHY_PIPE_NUM];
    TS_S32 s32WorkingViNum;
} TS_Common_ViAttr_t;

typedef struct TS_Common_VencAttr_s
{
    TS_BOOL bEnable;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VENC_CHN VencChnId;
    TS_U32 u32Width;
    TS_U32 u32Height;
    PAYLOAD_TYPE_E eVencType;
    VENC_RC_MODE_E eRcMode;
    TS_U32 u32Gop;
    TS_U32 u32StatTime;
    TS_U32 u32SrcFrameRate;
    TS_U32 u32Profile;
    TS_U32 u32BitRate;
    TS_U32 u32MaxQp;
    TS_U32 u32MinQp;
    TS_U32 u32Qfactor;
    TS_U32 u32Bufsize;
    TS_U32 s32IPQPDelta;
    TS_U32 u32MaxISize;
    TS_U32 u32MaxPSize;
    TS_Common_Venc_DataCb datacb;
} TS_Common_VencAttr_t;

typedef struct TS_Common_VideoAttr_s
{
    TS_Common_ViAttr_t ViAttr;
    TS_Common_VpssAttr_t *pVpssAttr;
    TS_Common_VencAttr_t *pVencAttr;
} TS_Common_VideoAttr_t;

typedef struct TS_Common_AudioAttr_s
{
    AUDIO_DEV AiDevId;
    AUDIO_DEV AoDevId;
    PAYLOAD_TYPE_E enFormat;
    AUDIO_SOUND_MODE_E enSoundMode;
    AUDIO_SAMPLE_RATE_E enSampleRate;
    TS_U32 u32PeriodSize;
    AUDIO_BIT_WIDTH_E enBitwidth;
    TS_BOOL bAecEnable;
    TS_BOOL bAiVqeEnable;
    TS_BOOL bAnrEnable;
    TS_BOOL bAgcEnable;
    TS_BOOL bInterleaved;
    AIO_MODE_E enChannelMode;
} TS_Common_AudioAttr_t;

// // osd
// #define MAX_DLA_RECT_NUMBER 10
// #define MAX_RECT_LITS_NUMBER 10

// #define MAX_RGN_NUMBER_PER_CHN 16

// #define HZ_8P_BIN_SIZE 65424
// #define HZ_12P_BIN_SIZE 196272
// #define HZ_16P_BIN_SIZE 261696
// typedef unsigned short RGBA4444;
// #define MAX_VIDEO_NUMBER 6

// #define OSD_TEXT_SMALL_FONT_SIZE FONT_SIZE_16
// #define OSD_TEXT_MEDIUM_FONT_SIZE FONT_SIZE_32
// #define OSD_TEXT_LARGE_FONT_SIZE FONT_SIZE_72

// #define OSD_COLOR_INVERSE_THD 96
// #define RGN_PALETTEL_TABLE_ALPHA_INDEX 0x00

// typedef enum _PixelFormat_e
// {
//     COLOR_FormatUnused,
//     COLOR_FormatYCbYCr,              // yuv422
//     COLOR_FormatSstarSensor16bitRaw, //
//     COLOR_FormatSstarSensor16bitYC,
//     COLOR_FormatSstarSensor16bitSTS, // do not support
//     COLOR_FormatYUV420SemiPlanar,
//     COLOR_FormatYUV420Planar, // yuv420
//     COLOR_Format16bitBGR565,
//     COLOR_Format16bitARGB4444,
//     COLOR_Format16bitARGB1555,
//     COLOR_Format24bitRGB888,
//     COLOR_Format32bitABGR8888,
//     COLOR_FormatL8,
//     COLOR_FormatMONO,
//     COLOR_FormatGRAY2,
//     COLOR_FormatMax,
// } PixelFormat_e;

// typedef enum _OsdFontSize_e
// {
//     FONT_SIZE_8,
//     FONT_SIZE_12,
//     FONT_SIZE_16,
//     FONT_SIZE_24,
//     FONT_SIZE_32,
//     FONT_SIZE_36,
//     FONT_SIZE_40,
//     FONT_SIZE_48,
//     FONT_SIZE_56,
//     FONT_SIZE_60,
//     FONT_SIZE_64,
//     FONT_SIZE_72,
//     FONT_SIZE_80,
//     FONT_SIZE_84,
//     FONT_SIZE_96
// } OsdFontSize_e;

// typedef struct ImageData_s
// {
//     TS_U16 width;
//     TS_U16 height;
//     TS_U8 *buffer;
// } ImageData_t;

// typedef struct _Color_t
// {
//     TS_U8 a;
//     TS_U8 r;
//     TS_U8 g;
//     TS_U8 b;
// } Color_t;

// typedef struct _Point_t
// {
//     TS_S32 x;
//     TS_S32 y;
// } Point_t;

// typedef struct _YUVColor_t
// {
//     TS_U8 y;
//     TS_U8 u;
//     TS_U8 v;
//     TS_U8 transparent;
// } YUVColor_t;

// typedef struct _TextWidgetAttr_s
// {
//     const char *string;
//     Point_t *pPoint;
//     OsdFontSize_e size;
//     TS_RGN_PixelFormat_e pmt;
//     Color_t *pfColor;
//     Color_t *pbColor;
//     TS_U8 u32Color;
//     TS_U32 space;
//     TS_BOOL bHard;
//     TS_BOOL bRle;
//     TS_BOOL bOutline;
// } TextWidgetAttr_t;

// typedef struct _RectWidgetAttr_s
// {
//     TS_SYS_WindowRect_t *pstRect;
//     TS_S32 s32RectCnt;
//     TS_U32 u32Color;
//     TS_U8 u8BorderWidth;
//     TS_RGN_PixelFormat_e pmt;
//     Color_t *pfColor;
//     Color_t *pbColor;
//     TS_BOOL bFill;
//     TS_BOOL bHard;
//     TS_BOOL bOutline;
// } RectWidgetAttr_t;

// typedef struct TS_Font_s
// {
//     TS_U32 nFontSize;
//     TS_U8 *pData;
// } TS_Font_t;

// typedef enum
// {
//     HZ_DOT_8,
//     HZ_DOT_12,
//     HZ_DOT_16,
//     HZ_DOT_NUM
// } TS_FontDot_e;


// typedef struct _rect{
//     struct list_head rectlist;
//     TS_S32  tCount;
//     TS_U8 *pChar;
// } TS_RectList_t;
void TS_COMMON_SYS_ShowVersion();
TS_S32 TS_COMMON_SYS_BwLimitInit(TS_Common_ViAttr_t *pstViAttr);
TS_S32 TS_COMMON_SYS_Init();
TS_S32 TS_COMMON_MMZ_Init();
TS_VOID TS_COMMON_SYS_Exit(void);
char *TS_Common_SysMmap(unsigned long long u64PhyAddr, unsigned int mapsize);
void TS_Common_SysMunmap(void *pVirtualAddress, unsigned int mapsize);
TS_S32 TS_Common_SysMmz_Alloc(unsigned char *pstMMAHeapName, unsigned int u32BlkSize, unsigned long long *phyAddr, unsigned long long *virAddr);
void TS_Common_SysMmz_Free(unsigned long long phyAddr, unsigned long long *virAddr);

TS_S32 TS_COMMON_VI_StartVi_And_Aiisp(TS_Common_ViAttr_t *pstViConfig);
TS_S32 TS_COMMON_VI_StopVi_And_Aiisp(TS_Common_ViAttr_t *pstViConfig);
TS_S32 TS_COMMON_VI_Aiisp_SetAutoAttr(TS_U32 u32Width);
TS_S32 TS_COMMON_VI_StartVi(TS_Common_ViAttr_t *pstViConfig);
TS_S32 TS_COMMON_VI_StopVi(TS_Common_ViAttr_t *pstViConfig);

TS_S32 TS_Common_IspLoadIq(VI_PIPE ViPipe, char *filepath);
TS_S32 TS_Common_IspIqStart(void);
TS_S32 TS_Common_IspIqStop(void);

TS_S32 TS_Common_IspBrightnessSet(VI_PIPE ViPipe, int brightness);
TS_S32 TS_Common_IspSharpnessSet(VI_PIPE ViPipe, int sharpness);
TS_S32 TS_Common_IspSaturationSet(VI_PIPE ViPipe, int saturation);
TS_S32 TS_Common_IspContrastSet(VI_PIPE ViPipe, int contrast);
TS_S32 TS_Common_IspForceFlickerSet(VI_PIPE ViPipe, int forceFlicker);
TS_S32 TS_Common_IspFilckerSet(VI_PIPE ViPipe, int Hz);
TS_S32 TS_Common_IspShutterSet(VI_PIPE ViPipe, int minShutter, int maxShutter);
TS_S32 TS_Common_IspMaxGainSet(VI_PIPE ViPipe, int gain);
TS_S32 TS_Common_IspAWBSet(VI_PIPE ViPipe, int whitebalance);
TS_S32 TS_Common_IspBLCGet(VI_PIPE ViPipe, char *backlight);
TS_S32 TS_Common_IspBLCSet(VI_PIPE ViPipe, int backlight);
TS_S32 TS_Common_IspHLCSet(VI_PIPE ViPipe, int hlc);
TS_S32 TS_Common_ISP2DnrSet(VI_PIPE ViPipe, char *oriTnf, int tnf);
TS_S32 TS_Common_ISP3DnrSet(VI_PIPE ViPipe, char *oriSnf, int snf);
TS_S32 TS_Common_ISPWeightSet(VI_PIPE ViPipe, int weight);
TS_S32 TS_Common_IspWdrValueGet(VI_PIPE ViPipe, char *wdr_value);
TS_S32 TS_Common_IspWdrValueSet(VI_PIPE ViPipe, char *oriWdrValue, int wdr_value);
TS_S32 TS_Common_IspFpsSet(VI_PIPE ViPipe, int fps);
/* daynight -> DAY_MODE:1 NIGHT_MODE:0 (SetParamIndex 0=day, 1=night) */
TS_S32 TS_Common_IspDayNightSet(VI_PIPE ViPipe, int daynight);
TS_S32 TS_Common_IspDayNightGet(VI_PIPE ViPipe, int *daynight);
TS_S32 TS_Common_IspAeTargetYGet(VI_PIPE ViPipe, unsigned int *u32Y);
TS_S32 TS_Common_IspAeTargetYSet(VI_PIPE ViPipe, unsigned int *u32Y);
TS_S32 TS_Common_IspSkipFrameSet(VI_PIPE ViPipe, int FrameCnt);
TS_S32 TS_Common_SensorMirrorSet(VI_PIPE ViPipe, TS_BOOL bHFlip, TS_BOOL bVFlip);

TS_S32 TS_COMMON_VPSS_Start(VPSS_GRP VpssGrp, VPSS_GRP_ATTR_S *pstVpssGrpAttr, VPSS_CHN_ATTR_S *pastVpssChnAttr, TS_S32 chnlNum);
TS_S32 TS_COMMON_VPSS_Stop(VPSS_GRP VpssGrp, TS_S32 chnlNum);
TS_S32 TS_COMMON_VPSS_ChnCrop(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, TS_S32 x, TS_S32 y, TS_U32 width, TS_U32 height);
TS_S32 TS_COMMON_VPSS_GrpCrop(VPSS_GRP VpssGrp, TS_S32 x, TS_S32 y, TS_U32 width, TS_U32 height);

TS_S32 TS_COMMON_VI_Bind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp);
TS_S32 TS_COMMON_VI_UnBind_VPSS(VI_PIPE ViPipe, VI_CHN ViChn, VPSS_GRP VpssGrp);
TS_S32 TS_COMMON_VPSS_Bind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe);
TS_S32 TS_COMMON_VPSS_UnBind_CPM(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, CPM_GRP CpmGrp, CPM_PIPE CpmPipe);
TS_S32 TS_COMMON_CPM_Bind_VENC(CPM_GRP CpmGrp, CPM_CHN CpmChn, VENC_CHN VencChn);
TS_S32 TS_COMMON_CPM_UnBind_VENC(CPM_GRP CpmGrp, CPM_CHN CpmChn, VENC_CHN VencChn);
TS_S32 TS_COMMON_VPSS_Bind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn);
TS_S32 TS_COMMON_VPSS_UnBind_VENC(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, VENC_CHN VencChn);

TS_S32 TS_Common_VencCreateChannel(TS_Common_VencAttr_t *pVencAttr);
TS_S32 TS_Common_VencGetStream(TS_Common_VencAttr_t *pstVencAttr, int *bStart);
TS_S32 TS_Common_VencGetMaxFd(TS_Common_VencAttr_t *pstVencAttr);
TS_S32 TS_Common_VencSetGop(VENC_CHN VencChn, int gop);
TS_S32 TS_Common_VencSetBitrate(VENC_CHN VencChn, int bitrate);
TS_S32 TS_Common_VencSetFps(VENC_CHN VencChn, int fps);
TS_S32 TS_Common_VencRequestIdr(VENC_CHN VencChn);
TS_S32 TS_Common_VencSetChnAttr(VENC_CHN VencChn, TS_Common_VencAttr_t *pVencAttr);
TS_S32 TS_Common_VENC_Stop(VENC_CHN VencChn);
TS_S32 TS_Common_VENC_SnapStart(VENC_CHN VencChn, SIZE_S *pstSize, TS_BOOL bSupportDCF);
TS_S32 TS_Common_VENC_SnapStop(VENC_CHN VencChn);

TS_S32 TS_Common_OsdCanvasUpdate(RGN_HANDLE hHandle);
TS_S32 TS_Common_OsdCanvasGet(RGN_HANDLE hHandle, RGN_CANVAS_INFO_S **ppstRgnCanvasInfo);
TS_S32 TS_Common_OsdAttrGet(RGN_HANDLE hHandle, RGN_ATTR_S *pstRgnAttr);
TS_S32 TS_Common_OsdDisplayAttrGet(RGN_HANDLE hHandle, MPP_CHN_S *pstChnPort, RGN_CHN_ATTR_S *pstChnPortAttr);
TS_VOID TS_Common_OsdSetBitmap(RGN_HANDLE hHandle, PIXEL_FORMAT_E enPix, TS_U32 u32W, TS_U32 u32H, MPP_CHN_S *pstRgnChnPort);
TS_S32 TS_Common_OsdCreate(RGN_HANDLE hHandle, RGN_ATTR_S *pstRgnAttr, MPP_CHN_S *pstRgnChnPort,
                           RGN_CHN_ATTR_S *pstRgnChnPortParam);
TS_S32 TS_Common_OsdDestory(RGN_HANDLE hHandle, MPP_CHN_S *pstRgnChnPort);

TS_S32 TS_Common_AudioAi_SetVolume(AUDIO_DEV AiDevId, AI_CHN AiChnId, TS_S32 s32VolumeDb);
TS_S32 TS_Common_AudioAo_SetVolume(AUDIO_DEV AoDevId, TS_S32 s32VolumeDb);
TS_S32 TS_Common_AudioAo_SetMute(AUDIO_DEV AoDevId, int enable);
TS_S32 TS_Common_AudioAi_GetStream(AUDIO_DEV AiDevId, AI_CHN AiChnId, anj_mw_media_audio_pcm_data pcm_data_cb, int *bStart);
TS_S32 TS_Common_AudioAo_PcmPlay(char *data, int len);
TS_S32 TS_Common_AudioAo_PlayEndingCheck(void);
TS_S32 TS_Common_AudioAi_SetTalkVqeAttr(AUDIO_DEV AiDevId, AI_CHN AiChnId, AIO_ATTR_S *pstAioAttr,
                                        AUDIO_VQE_CONFIG_S *pstAiVqeAttr);
TS_S32 TS_Common_AudioAiInit(TS_Common_AudioAttr_t *pstAudioAttr);
TS_S32 TS_Common_AudioAiUnInit(void);
TS_S32 TS_Common_AudioAoInit(TS_Common_AudioAttr_t *pstAudioAttr);
TS_S32 TS_Common_AudioAoUnInit(void);

#ifdef __cplusplus
}
#endif

#endif //_TSML_MEDIA_COMMON_H_
