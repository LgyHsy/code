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

#include "mi_sys.h"
#include "mi_sensor.h"
#include "mi_vif.h"
#include "mi_isp.h"
#include "mi_scl.h"
#include "mi_venc.h"
#include "mi_ldc.h"
#include "mi_ai.h"
#include "mi_ao.h"
#include "mi_rgn.h"
#include "mi_isp_ae.h"
#include "mi_isp_iq.h"
#include "mi_ipu.h"

#include "AudioProcess.h"
#include "AudioAecProcess.h"
#include "AudioBfProcess.h"

#include "anj_mw_comm.h"
#include "anj_mw_log.h"
#include "anj_mw_mem.h"
#include "anj_mw_list.h"
#include "anj_mw_media_audio.h"

#define DEFAULT_SENSOR_FPS (15)
#define ST_AUDIO_PLAY_PERIOD_SIZE    (1024)
#define AIISP_MAX_FPS      (12)

#ifndef ExecFunc
#define ExecFunc(_func_, _ret_)                                \
do                                                         \
{                                                          \
    MI_S32 s32Ret = MI_SUCCESS;                            \
    s32Ret = _func_;                                       \
    if (s32Ret != _ret_)                                   \
    {                                                      \
        __ERR("exec function failed, error:%x\n", s32Ret); \
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
    MI_S32 s32Ret = MI_SUCCESS;                            \
    s32Ret = _func_;                                       \
    if (s32Ret != MI_SUCCESS)                              \
    {                                                      \
        __ERR("exec function failed, error:%x\n", s32Ret); \
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

#ifndef MALLOC
#define MALLOC(s) anj_mw_malloc(s)
#endif

#ifndef FREEIF
#define FREEIF(m)       \
if (m != 0)         \
{                   \
    anj_mw_free(m); \
    m = NULL;       \
}
#endif

#ifndef MI_SYS_Malloc
#define MI_SYS_Malloc(size) anj_mw_malloc(size)
#endif

#ifndef MI_SYS_Realloc
#define MI_SYS_Realloc(ptr, size) anj_mw_realloc(ptr, size)
#endif

#ifndef MI_SYS_Free
#define MI_SYS_Free(pData)      \
{                           \
    if (pData != NULL)      \
        anj_mw_free(pData); \
    pData = NULL;           \
}
#endif

#ifndef RGB2PIXEL1555
#define RGB2PIXEL1555(a, r, g, b) (((a & 0x80) << 8) | ((r & 0xF8) << 7) | ((g & 0xF8) << 2) | ((b & 0xF8) >> 3))
#endif

#define MI_FAILED (-1)

#define MAX_VDF_NUM_PER_CHN 16
#define NALU_PACKET_SIZE 512 * 1024

typedef struct
{
    MI_U32 u32X;
    MI_U32 u32Y;
} ST_Point_T;

typedef struct ST_Sys_BindInfo_s
{
    MI_SYS_ChnPort_t stSrcChnPort;
    MI_SYS_ChnPort_t stDstChnPort;
    MI_U32 u32SrcFrmrate;
    MI_U32 u32DstFrmrate;
    MI_SYS_BindType_e eBindType;
    MI_U32 u32BindParam;
} ST_Sys_BindInfo_T;

typedef struct ST_Common_SclStartParam_s
{
    MI_SCL_DEV SclDevId;
    MI_SCL_CHANNEL SclChnId;
    MI_SCL_PORT SclOutPortId;
    MI_SYS_WindowRect_t stSclChnCropInfo;
    MI_SYS_WindowRect_t stSclOutputPortCropRect;
    MI_SYS_WindowSize_t stSclOutputSize;
    MI_SYS_PixelFormat_e ePixelFormat;
} ST_Common_SclStartParam_t;

typedef struct ST_Common_VifAttr_s
{
    MI_VIF_GROUP VifGroupId;
    MI_VIF_DEV VifDevId;
    MI_VIF_PORT VifOutPortId;
} ST_Common_VifAttr_t;

typedef struct ST_Common_IspAttr_s
{
    MI_ISP_DEV IspDevId;
    MI_ISP_CHANNEL IspChnId;
    MI_ISP_PORT IspOutPortId;
    MI_U32 u32SensorBindId;
    MI_SYS_PixelFormat_e ePixelFormat;
    MI_ISP_AE_ExpoInfoType_t IspAeInfo;
} ST_Common_IspAttr_t;

typedef void (* ST_Common_Scl_DataCb)(int u32DevId, void *p_vir_addr, unsigned long long p_phy_addr, int len, void *param);

typedef struct ST_Common_SclAttr_s
{
    MI_BOOL bEnable;
    MI_SCL_DEV SclDevId;
    MI_SCL_CHANNEL SclChnId;
    MI_SCL_PORT SclOutPortId;
    MI_SYS_WindowSize_t stSCLOutputSize;
    MI_U32 u32SrcFrmRateNum;
    MI_BOOL bMirror;
    MI_BOOL bFlip;
    MI_U32 u32DepthSet;
    ST_Common_Scl_DataCb datacb;
    int isjpeg;
    void *param;
} ST_Common_SclAttr_t;

typedef void (* ST_Common_Venc_DataCb)(int VencChn, int iskey, char *data, int len, unsigned int u32Seq, int codec, unsigned long long int timestamp);

typedef struct ST_Common_VencAttr_s
{
    MI_BOOL bEnable;
    MI_VENC_DEV VencDevId;
    MI_VENC_CHN VencChnId;
    MI_SYS_WindowSize_t stVencRes;
    MI_VENC_ModType_e eVencType;
    MI_VENC_RcMode_e eRcMode;
    MI_U32 u32Gop;
    MI_U32 u32StatTime;
    MI_U32 u32SrcFrmRateNum;
    MI_U32 u32Profile;
    MI_U32 u32BitRate;
    MI_U32 u32MaxQp;
    MI_U32 u32MinQp;
    MI_U32 u32Qfactor;
    MI_U32 u32Bufsize;
    MI_U32 s32IPQPDelta;
    MI_U32 u32MaxISize;
    MI_U32 u32MaxPSize;
    ST_Common_Venc_DataCb datacb;
} ST_Common_VencAttr_t;

typedef struct ST_Common_VideoAttr_s
{
    MI_SNR_PADID SnrPadId;
    ST_Common_VifAttr_t VifAttr;
    ST_Common_IspAttr_t IspAttr;
    ST_Common_SclAttr_t SclAttr[MAX_SCL_PORT];
    ST_Common_VencAttr_t *pVencAttr;
} ST_Common_VideoAttr_t;

typedef struct ST_Common_AudioAttr_s
{
    MI_AUDIO_DEV AiDevId;
    MI_U8 u8ChnGrpIdx;
    MI_U8 u8ChnGrpId;
    MI_AUDIO_DEV AoDevId;
    MI_AUDIO_Format_e enFormat;
    MI_AUDIO_SoundMode_e enSoundMode;
    MI_AUDIO_SampleRate_e enSampleRate;
    MI_U32 u32PeriodSize;
    MI_BOOL bInterleaved;
    MI_AO_ChannelMode_e enChannelMode;
} ST_Common_AudioAttr_t;

// osd
#define MAX_DLA_RECT_NUMBER 10
#define MAX_RECT_LIST_NUMBER 10

#define MAX_RGN_NUMBER_PER_CHN 16

#define HZ_8P_BIN_SIZE 65424
#define HZ_12P_BIN_SIZE 196272
#define HZ_16P_BIN_SIZE 261696
typedef unsigned short RGBA4444;
#define MAX_VIDEO_NUMBER 6

#define OSD_TEXT_SMALL_FONT_SIZE FONT_SIZE_16
#define OSD_TEXT_MEDIUM_FONT_SIZE FONT_SIZE_32
#define OSD_TEXT_LARGE_FONT_SIZE FONT_SIZE_72

#define OSD_COLOR_INVERSE_THD 96
#define RGN_PALETTEL_TABLE_ALPHA_INDEX 0x00

typedef enum _PixelFormat_e
{
    COLOR_FormatUnused,
    COLOR_FormatYCbYCr,              // yuv422
    COLOR_FormatSstarSensor16bitRaw, //
    COLOR_FormatSstarSensor16bitYC,
    COLOR_FormatSstarSensor16bitSTS, // do not support
    COLOR_FormatYUV420SemiPlanar,
    COLOR_FormatYUV420Planar, // yuv420
    COLOR_Format16bitBGR565,
    COLOR_Format16bitARGB4444,
    COLOR_Format16bitARGB1555,
    COLOR_Format24bitRGB888,
    COLOR_Format32bitABGR8888,
    COLOR_FormatL8,
    COLOR_FormatMONO,
    COLOR_FormatGRAY2,
    COLOR_FormatMax,
} PixelFormat_e;

typedef enum _OsdFontSize_e
{
    FONT_SIZE_8,
    FONT_SIZE_12,
    FONT_SIZE_16,
    FONT_SIZE_24,
    FONT_SIZE_32,
    FONT_SIZE_36,
    FONT_SIZE_40,
    FONT_SIZE_48,
    FONT_SIZE_56,
    FONT_SIZE_60,
    FONT_SIZE_64,
    FONT_SIZE_72,
    FONT_SIZE_80,
    FONT_SIZE_84,
    FONT_SIZE_96
} OsdFontSize_e;

typedef struct ImageData_s
{
    MI_U16 width;
    MI_U16 height;
    MI_U8 *buffer;
} ImageData_t;

typedef struct _Color_t
{
    MI_U8 a;
    MI_U8 r;
    MI_U8 g;
    MI_U8 b;
} Color_t;

typedef struct _Point_t
{
    MI_S32 x;
    MI_S32 y;
} Point_t;

typedef struct _YUVColor_t
{
    MI_U8 y;
    MI_U8 u;
    MI_U8 v;
    MI_U8 transparent;
} YUVColor_t;

typedef struct _TextWidgetAttr_s
{
    const char *string;
    Point_t *pPoint;
    OsdFontSize_e size;
    MI_RGN_PixelFormat_e pmt;
    Color_t *pfColor;
    Color_t *pbColor;
    MI_U8 u32Color;
    MI_U32 space;
    MI_BOOL bHard;
    MI_BOOL bRle;
    MI_BOOL bOutline;
} TextWidgetAttr_t;

typedef struct _RectWidgetAttr_s
{
    MI_SYS_WindowRect_t *pstRect;
    MI_S32 s32RectCnt;
    MI_U32 u32Color;
    MI_U8 u8BorderWidth;
    MI_RGN_PixelFormat_e pmt;
    Color_t *pfColor;
    Color_t *pbColor;
    MI_BOOL bFill;
    MI_BOOL bHard;
    MI_BOOL bOutline;
} RectWidgetAttr_t;

typedef struct MI_Font_s
{
    MI_U32 nFontSize;
    MI_U8 *pData;
} MI_Font_t;

typedef enum
{
    HZ_DOT_8,
    HZ_DOT_12,
    HZ_DOT_16,
    HZ_DOT_NUM
} MI_FontDot_e;

typedef struct
{
    MI_U32 u16X;
    MI_U32 u16Y;
} DrawPoint_t;

typedef struct
{
    MI_U32 u16Width;
    MI_U32 u16Height;
} DrawSize_t;

typedef struct
{
    MI_RGN_PixelFormat_e ePixelFmt;
    MI_U32 u32Color;
} DrawRgnColor_t;

typedef struct _rect{
    struct list_head rectlist;
    MI_S32  tCount;
    MI_U8 *pChar;
} ST_RectList_t;

int ST_Common_DumpFile(MI_SYS_ChnPort_t *pstChnPort, char *FileName);

MI_S32 ST_Common_SysInit(void);
MI_S32 ST_Common_SysUnInit(void);
MI_S32 ST_Common_SysBind(ST_Sys_BindInfo_T *pstBindInfo);
MI_S32 ST_Common_SysUnBind(ST_Sys_BindInfo_T *pstBindInfo);
char *ST_Common_SysMmap(unsigned long long u64PhyAddr, unsigned int mapsize);
void ST_Common_SysMunmap(void *pVirtualAddress, unsigned int mapsize);
MI_S32 ST_Common_SysMma_Alloc(unsigned char *pstMMAHeapName, unsigned int u32BlkSize ,unsigned long long *phyAddr);
void ST_Common_SysMma_Free(unsigned long long phyAddr);

MI_S32 ST_Common_SensorInit(MI_SNR_PADID eSnrPad, MI_U32 u32Fps, MI_U8 u8ResIdx);
MI_S32 ST_Common_SensorUnInit(MI_SNR_PADID eSnrPad);
MI_S32 ST_Common_SensorGetRectInfo(MI_SNR_PADID eSnrPad, MI_SYS_WindowRect_t *pstSnrRes);
MI_S32 ST_Common_SensorMirrorSet(MI_SNR_PADID eSnrPadId, MI_BOOL bHFlip, MI_BOOL bVFlip);
MI_S32 ST_Common_SensorFpsGet(MI_SNR_PADID eSnrPadId, int *fps);
MI_S32 ST_Common_SensorFpsSet(MI_SNR_PADID eSnrPadId, int fps);

MI_S32 ST_Common_VifInit(ST_Common_VifAttr_t *pstVifAttr, MI_SNR_PADID eSnrPadId);
MI_S32 ST_Common_VifUnInit(ST_Common_VifAttr_t *pstVifAttr);
MI_S32 ST_Common_VifOutputPortDisable(ST_Common_VifAttr_t *pstVifAttr);
MI_S32 ST_Common_VifOutputPortEnable(ST_Common_VifAttr_t *pstVifAttr);

MI_S32 ST_Common_IspInit(MI_ISP_DEV IspDevId);
MI_S32 ST_Common_IspUnInit(MI_ISP_DEV IspDevId);
MI_S32 ST_Common_IspStart(ST_Common_IspAttr_t *pstIspAttr, MI_SYS_WindowRect_t *pSnrRes);
MI_S32 ST_Common_IspStop(ST_Common_IspAttr_t *pstIspAttr);
MI_S32 ST_Common_IspAiStart(ST_Common_IspAttr_t *pstIspAttr, int aiIspMode);
MI_S32 ST_Common_IspAiStop(ST_Common_IspAttr_t *pstIspAttr);

MI_S32 ST_Common_IspParaInit(MI_ISP_CHANNEL IspChnId);
MI_S32 ST_Common_IspCus3aInit(MI_ISP_CHANNEL IspChnId);
MI_S32 ST_Common_IspLoadIq(MI_ISP_CHANNEL IspChnId, char *filepath);
MI_S32 ST_Common_IspIqStart();
MI_S32 ST_Common_IspIqStop();
MI_S32 ST_Common_Isp_AiCali_Load(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel);
void ST_Common_IspAeInfoGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_AE_ExpoInfoType_t *pstAeData);
void ST_Common_IspDetectionSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int D2NThd, int N2DThd);

MI_S32 ST_Common_IspBrightnessSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int brightness);
MI_S32 ST_Common_IspSharpnessGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_IQ_SharpnessType_t *data);
MI_S32 ST_Common_IspSharpnessSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel,
                                 char *sharpness0, char *sharpness1, char *sharpness2, int sharpness);
MI_S32 ST_Common_IspSaturationGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_IQ_SaturationType_t *data);
MI_S32 ST_Common_IspSaturationSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriSaturation, int saturation);
MI_S32 ST_Common_IspContrastSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int contrast);
MI_S32 ST_Common_IspFilckerSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int Hz);

MI_S32 ST_Common_IspShutterSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int minShutter, int maxShutter);
MI_S32 ST_Common_IspMaxGainSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int gain);

MI_S32 ST_Common_IspAWBSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int whitebalance);
MI_S32 ST_Common_IspBLCGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *backlight);
MI_S32 ST_Common_IspBLCSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriBacklight, int backlight);
MI_S32 ST_Common_IspHLCSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int hlc, int brightness);

MI_S32 ST_Common_ISP2DnrGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *tnf);
MI_S32 ST_Common_ISP2DnrSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriTnf, int tnf);
MI_S32 ST_Common_ISP3DnrGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *snf);
MI_S32 ST_Common_ISP3DnrSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriSnf, int snf);
MI_S32 ST_Common_ISPWeightSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int weight);
MI_S32 ST_Common_IspWdrValueGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *wdr_value);
MI_S32 ST_Common_IspWdrValueSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, char *oriWdrValue, int wdr_value);
MI_S32 ST_Common_IspWdrEnableSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int enable);
MI_S32 ST_Common_IspRotateSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int rotate);
//daynight -> DAY_MODE:1 NIGHT_MODE:2
MI_S32 ST_Common_IspDayNightSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int daynight);
MI_S32 ST_Common_IspShutterusGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *maxshutterus, unsigned int *minshutterus);
MI_S32 ST_Common_IspShutterusSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int maxshutterus, unsigned int minshutterus);

MI_S32 ST_Common_IspAeTargetYGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32Y);
MI_S32 ST_Common_IspAeTargetYSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32Y);

MI_S32 ST_Common_IspConvergeSpeedGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32SpeedY);
MI_S32 ST_Common_IspConvergeSpeedSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, unsigned int *u32SpeedY);

MI_S32 ST_Common_IspZoomStop(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel);
MI_S32 ST_Common_IspZoomStart(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomAttr_t *pstZoomAttr);
MI_S32 ST_Common_IspZoomTableLoad(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomTable_t *pZoomTable);
MI_S32 ST_Common_IspZoomCurGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, MI_ISP_ZoomAttr_t *pstZoomAttr);

MI_U8 ST_Common_IspTempGet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel);
MI_S32 ST_Common_IspSkipFrameSet(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel, int FrameCnt);
MI_S32 ST_Common_IspChannelStop(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel);
MI_S32 ST_Common_IspChannelStart(MI_ISP_DEV DevId, MI_ISP_CHANNEL Channel);

MI_S32 ST_Common_SclInit(MI_SCL_DEV SclDevId);
MI_S32 ST_Common_SclUnInit(MI_SCL_DEV SclDevId);

MI_S32 ST_Common_SclPortStart(ST_Common_SclStartParam_t *pstSclStartParam, MI_SCL_OutPortParam_t *pstSclOutputParam);
MI_S32 ST_Common_SclPortFrmrateSet(ST_Common_SclStartParam_t *pstSclStartParam, MI_U32 u32SrcFrmrate, MI_U32 u32DstFrmrate);
MI_S32 ST_Common_SclPortDepthSet(ST_Common_SclStartParam_t *pstSclStartParam, MI_U32 u32Depth);
MI_S32 ST_Common_SclStart(ST_Common_SclStartParam_t *pstSclStartParam);
MI_S32 ST_Common_SclPortStop(ST_Common_SclAttr_t *pSclAttr);
MI_S32 ST_Common_SclChnStop(ST_Common_SclAttr_t *pSclAttr);
MI_S32 ST_Common_SclGetYuv(MI_SYS_ChnPort_t *pstChnPort, ST_Common_Scl_DataCb datacb, void *param, int *bStart);
MI_S32 ST_Common_SclCropSet(ST_Common_SclAttr_t *pSclAttr, MI_SYS_WindowRect_t *pstOutCropInfo);
MI_S32 ST_Common_SclPause(ST_Common_SclAttr_t *pSclAttr);
MI_S32 ST_Common_SclRecover(ST_Common_SclAttr_t *pSclAttr);


MI_S32 ST_Common_VencInit(ST_Common_VencAttr_t *pVencAttr);
MI_S32 ST_Common_VencUnInit(MI_VENC_DEV VeDev);
MI_S32 ST_Common_VencCreateChannel(ST_Common_VencAttr_t *pVencAttr);
MI_S32 ST_Common_VencGetStream(MI_VENC_CHN VencChn, MI_VENC_ModType_e eVencType, ST_Common_Venc_DataCb datacb, int *bStart);
MI_S32 ST_Common_VencSetGop(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int gop);
MI_S32 ST_Common_VencSetBitrate(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int bitrate);
MI_S32 ST_Common_VencSetFps(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, int fps);
MI_S32 ST_Common_VencRequestIdr(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn);
MI_S32 ST_Common_VencSetChnAttr(MI_VENC_DEV VencDevId, MI_VENC_CHN VencChn, ST_Common_VencAttr_t *pVencAttr);

MI_S32 ST_Common_AudioAo_SetMute(MI_AUDIO_DEV AoDevId, int enable);
MI_S32 ST_Common_AudioAi_SetMute(MI_AUDIO_DEV AiDevId, MI_U8 u8ChnGrpId, int enable);
MI_S32 ST_Common_AudioAo_SetVolume(MI_AUDIO_DEV AoDevId, int volume);
MI_S32 ST_Common_AudioAi_SetVolume(ST_Common_AudioAttr_t *pstAudioAttr, int volume);
MI_S32 ST_Common_AudioAi_GetStream(MI_AUDIO_DEV AiDevId, MI_U8 u8ChnGrpId, anj_mw_media_audio_pcm_data pcm_data_cb, int *bStart);
MI_S32 ST_Common_AudioAo_PcmPlay(char *data, int len);
MI_S32 ST_Common_AudioAo_AencPlay(char *data, int len);
MI_S32 ST_Common_AudioAiInit(ST_Common_AudioAttr_t *pstAudioAttr);
MI_S32 ST_Common_AudioAiUnInit(void);
MI_S32 ST_Common_AudioAoInit(ST_Common_AudioAttr_t *pstAudioAttr);
MI_S32 ST_Common_AudioAoUnInit(void);
MI_S32 ST_Common_AudioAo_PlayEndingCheck();

void DrawPoint(void *pBaseAddr, MI_U32 u32Stride, MI_U32 u32Height, MI_U32 u32Width, DrawPoint_t stPt, DrawRgnColor_t stColor);
// void DrawLine(void *pBaseAddr, MI_U32 u32Stride, DrawPoint_t stStartPt, DrawPoint_t stEndPt, MI_U8 u8BorderWidth, DrawRgnColor_t stColor);
void DrawRect(void *pBaseAddr, MI_U32 u32Stride, DrawPoint_t stLeftTopPt, DrawPoint_t stRightBottomPt, MI_U8 u8BorderWidth, DrawRgnColor_t stColor);

MI_S32 ST_Common_OsdCanvasUpdate(MI_RGN_HANDLE hHandle);
MI_S32 ST_Common_OsdCanvasGet(MI_RGN_HANDLE hHandle, MI_RGN_CanvasInfo_t **pstRgnCanvasInfo);
MI_S32 ST_Common_OsdAttrGet(MI_RGN_HANDLE hHandle, MI_RGN_Attr_t *pstRgnAttr);
MI_S32 ST_Common_OsdDisplayAttrGet(MI_RGN_HANDLE hHandle, MI_RGN_ChnPort_t *pstChnPort, MI_RGN_ChnPortParam_t *pstChnPortAttr);
MI_S32 ST_Common_OsdCreate(MI_RGN_HANDLE hHandle, MI_RGN_Attr_t *pstRgnAttr, MI_RGN_ChnPort_t *pstRgnChnPort, MI_RGN_ChnPortParam_t *pstRgnChnPortParam);
MI_S32 ST_Common_OsdDestory(MI_RGN_HANDLE hHandle, MI_RGN_ChnPort_t *pstRgnChnPort);
MI_S32 ST_Common_OsdRgnInit(int pixel_fmt);
MI_S32 ST_Common_OsdRgnUnInit();

#ifdef __cplusplus
}
#endif

#endif //_TSML_MEDIA_COMMON_H_
