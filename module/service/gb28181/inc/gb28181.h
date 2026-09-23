#ifndef __GB28181__H__
#define __GB28181__H__

#include "anj_mbuf.h"
#include "anj_record.h"
#include "anj_mw_comm.h"

#define gb28181_log(format, arg...) __INFO(format, ##arg)
#define MAX_RECORD_LIST_NUM 1000

typedef struct
{
    int hcPort;       // 平台端口
    char hcIP[32];    // 平台IP
    char hcName[128]; // 平台域名
    char hcID[32];    // 平台域名ID

    int lcPort;    // 本地端口
    char lcIp[32]; // 本地IP
    char lcId[32]; // 本地域名ID

    char Username[64]; // 用户名一般等于lcId
    char Pwd[32];      // 密码
    char AlarmId[32];  // 报警域名ID

    int expires;      // 注册有效期
    int keepAlive;    // 心跳周期
    int keepAliveNum; // 最大心跳超时次数
} Service_GB28181_Cfg;

/*!
 *  @struct	tagTimeInfo
 *  @brief	时间信息
 */
typedef struct tagTimeInfo
{
    unsigned short wYear;   /*!< 年                                     */
    unsigned short wMonth;  /*!< 月                                     */
    unsigned short wDay;    /*!< 日                                     */
    unsigned short wHour;   /*!< 时                                     */
    unsigned short wMinute; /*!< 分                                     */
    unsigned short wSecond; /*!< 秒                                     */
} TimeInfo, *LPTimeInfo;

/*!
 *  @struct	tagHistoryChannelPositionInfo
 *  @brief	历史通道定位参数信息
 */
typedef struct tagHistoryChannelPositionInfo
{
    TimeInfo cSeekTime; /*!< 定位时间                               */
    TimeInfo cEndTime;  /*!< 结束时间                               */
    int bDownload;      /*!< 是否下载                               */
} HistoryChannelPositionInfo, *LPHistoryChannelPositionInfo;

typedef int (*GB28181_Start_stream_CB)(int gb28181Id, int *bH265Type);
typedef int (*GB28181_Stop_stream_CB)(int gb28181Id);

/*!
 *  @struct	tagHistoryChannelInterfaceMultiType
 *  @brief	历史通道接口信息
 */
typedef struct
{
    GB28181_Start_stream_CB ifStartStreamCb; /*!< 开启码流接口            */
    GB28181_Stop_stream_CB ifStopStreamCb;   /*!< 关闭码流通道接口 */
} GB28181_stream_CB, *LPGB28181_stream_CB;

typedef int (*GB28181_Ctrl_ptz_CB)(char *cmdcode, int speed, int param1);

/*!
 *  @struct	tagHistoryChannelInterfaceMultiType
 *  @brief	历史通道接口信息
 */
typedef struct
{
    GB28181_Ctrl_ptz_CB ifCtrlPtzCb; /*!< 开始捕获接口      */
} GB28181_ptz_CB, *LPGB28181_ptz_CB;

/*
 * @brief
 *	查询下一条历史接口
 * @param LPquery_storage
 *	历史文件查询
 * @param pCount
 *	历史文件当天数量
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryQueryNextMultiType)(char *buf);

/*!
 *  @struct	tagHistoryQueryInterfaceMultiType
 *  @brief	历史查询接口信息
 */
typedef struct tagHistoryQueryInterfaceMultiType
{
    IF_HistoryQueryNextMultiType ifHistoryQuery; /*!< 历史查询*/
} HistoryQueryInterfaceMultiType, *LPHistoryQueryInterfaceMultiType;

/*
 * @brief
 *	历史通道开始捕获接口
 * @param hIndex
 *	历史通道句柄
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryChannelStartCapture)(unsigned long dwUserID, char *buf);

/*
 * @brief
 *	历史通道停止捕获接口
 * @param hIndex
 *	历史通道句柄
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryChannelStopCapture)(unsigned long dwUserID);

/*
 * @brief
 *	历史通道暂停捕获接口
 * @param dwUserID
 *	历史通道id
 * @param status
 *	历史通道回放状态
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryChannelPauseCapture)(unsigned long dwUserID, int status);

/*
 * @brief
 *	历史通道快放，仅传送I帧
 * @param hIndex
 *	历史通道句柄
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryChannelSetSpeed)(unsigned long dwUserID, unsigned long dwSpeed);

/*
 * @brief
 *	历史通道定位接口
 * @param hIndex
 *	历史通道句柄
 * @param pHistoryChannelPositionInfo
 *	定位信息
 * @return
 *	返回0表示成功, 否则表示错误码
 * @note
 *	接口类型: 阻塞式
 */
typedef int (*IF_HistoryChannelPosition)(unsigned long dwUserID, char *buf);

/*!
 *  @struct	tagHistoryChannelInterfaceMultiType
 *  @brief	历史通道接口信息
 */
typedef struct tagHistoryChannelInterfaceMultiTypeCB
{
    IF_HistoryChannelStartCapture ifHistoryChannelStartCapture; /*!< 开始捕获接口      */
    IF_HistoryChannelStopCapture ifHistoryChannelStopCapture;   /*!< 停止捕获接口       */
    IF_HistoryChannelPauseCapture ifHistoryChannelPauseCapture; /*!< 暂停捕获接口       */
    IF_HistoryChannelSetSpeed ifHistoryChannelSetSpeed;         /*!< 历史流快放，仅发送I帧   */
    IF_HistoryChannelPosition ifHistoryChannelPosition;         /*!< 历史通道定位接口      */
} HistoryChannelInterfaceMultiTypeCB, *LPHistoryChannelInterfaceMultiTypeCB;

#ifdef __cplusplus
extern "C"
{
#endif
    int Service_GB2818_HistoryQueryMultiTypeIf(const void *pInterface);
    int Service_GB2818_HistoryChannelMultiTypeIf(const void *pInterface);
    int Service_GB2818_SetStreamIf(LPGB28181_stream_CB pInterface);
    int Service_GB2818_SetPtzIf(LPGB28181_ptz_CB pInterface);

    /*****************************************************************************
     函 数 名  : Service_GB28181_AlarmEventDeal
        功能描述  : 报警事件处理函数
        输入参数  : alarmCode 报警类型
                                SYSTEM_TIME* alarmtime 报警时间
        输出参数  : 无
        返 回 值  :成功 0， 失败其他
        调用函数  :
        被调函数  :
    *****************************************************************************/
    int Service_GB28181_AlarmEventDeal(int alarmCode, SYSTEM_TIME *pAlarmtime);
    void Service_GB28181_UpdateAudioType();

    int Service_GB2818_PbSendVStream(REC_HANDLE pHandle, media_frame_info_t *pFrameInfo, pb_cb_event_e EventID);
    int Service_GB2818_SendVStream(void *pPoper, media_frame_info_t *pFrameInfo);

    int Service_GB2818_Open(Service_GB28181_Cfg *sip_info);

    int Service_GB2818_Close(void);

#ifdef __cplusplus
}
#endif

#endif
