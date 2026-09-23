#ifndef __RECORD_LOG_H__
#define __RECORD_LOG_H__
#include "anj_mw_log.h"

#if defined(__cplusplus)
extern "C"
{
#endif

    /*****************************************************************************
     函 数 名  : record_log_init
     功能描述  :初始化记录日志模块
     输入参数  :   无
     输出参数  :
     返 回 值  :  0成功，其他失败
    *****************************************************************************/
    int record_log_init(void);

    /*****************************************************************************
     函 数 名  : record_log_uninit
     功能描述  :退出记录日志模块
     输入参数  :   无
     输出参数  :
     返 回 值  :  0成功，其他失败
    *****************************************************************************/
    int record_log_uninit(void);

    /*****************************************************************************
     函 数 名  : record_log_flush_file
     功能描述  : 刷新日志文件函数
     输入参数  :  int bBlockTimes 堵塞次数，每次200ms, 大于0生效
     输出参数  :
     返 回 值  :  0成功，其他失败
    *****************************************************************************/
    int record_log_flush_file(int bBlockTimes);

    /*****************************************************************************
     函 数 名  : record_log_del_file
     功能描述  :删除日志文件
     输入参数  :
     输出参数  :
     返 回 值  :  0成功，其他失败
    *****************************************************************************/
    int record_log_del_file(void);

    /*****************************************************************************
     函 数 名  : record_log_get_file
     功能描述  :发送日志文件函数
     输入参数  :  int nLen 文件名长度
     输出参数  :  char* szLogFile   文件名
     返 回 值  :  0成功，其他失败
    *****************************************************************************/
    int record_log_get_file(char *szLogFile, int nLen);

    void record_log_print(unsigned int nLogLevel, const char *psFileName, const char *psFuncName, int line, const char *format, ...);

#if 0
#define __RECORD_LOG_DBG(format, arg...) printf(format, ##arg);
#define __RECORD_LOG_INFO(format, arg...) printf(format, ##arg);
#define __RECORD_LOG_EVENT(format, arg...) printf(format, ##arg);
#define __RECORD_LOG_WARN(format, arg...) printf(format, ##arg);
#define __RECORD_LOG_ERR(format, arg...) printf(format, ##arg);
#define __RECORD_LOG_FATAL(format, arg...) printf(format, ##arg);
#else
#define __RECORD_LOG_DBG(format, arg...) record_log_print(0, __FILE__, __func__, __LINE__, format, ##arg);
#define __RECORD_LOG_INFO(format, arg...) record_log_print(SLOG_LVL_TRACE, __FILE__, __func__, __LINE__, format, ##arg);
#define __RECORD_LOG_EVENT(format, arg...) record_log_print(SLOG_LVL_EVENT, __FILE__, __func__, __LINE__, format, ##arg);
#define __RECORD_LOG_WARN(format, arg...) record_log_print(SLOG_LVL_WARNING, __FILE__, __func__, __LINE__, format, ##arg);
#define __RECORD_LOG_ERR(format, arg...) record_log_print(SLOG_LVL_ERROR, __FILE__, __func__, __LINE__, format, ##arg);
#define __RECORD_LOG_FATAL(format, arg...) record_log_print(SLOG_LVL_FATAL, __FILE__, __func__, __LINE__, format, ##arg);
#endif

#if defined(__cplusplus)
}
#endif

#endif
