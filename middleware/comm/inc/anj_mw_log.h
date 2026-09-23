#ifndef _ANJ_MW_LOG_H_
#define _ANJ_MW_LOG_H_

#include <time.h>
#include <stdio.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/* color setting */
#define DEBUG_COLOR_NORMAL          "\033[m"
#define DEBUG_COLOR_BLACK           "\033[30m"
#define DEBUG_COLOR_RED             "\033[31m"
#define DEBUG_COLOR_GREEN           "\033[32m"
#define DEBUG_COLOR_YELLOW          "\033[33m"
#define DEBUG_COLOR_BLUE            "\033[34m"
#define DEBUG_COLOR_PURPLE          "\033[35m"
#define DEBUG_COLOR_BKRED           "\033[41;37m"

#define SLOG_LVL_FATAL      (16)  /* 1<<4 */
#define SLOG_LVL_ERROR      (8)   /* 1<<3 */
#define SLOG_LVL_WARNING    (4)   /* 1<<2 */
#define SLOG_LVL_EVENT      (2)   /* 1<<1 */
#define SLOG_LVL_TRACE      (1)
#define SLOG_LVL_ALL        (SLOG_LVL_TRACE|SLOG_LVL_EVENT|SLOG_LVL_WARNING|SLOG_LVL_ERROR|SLOG_LVL_FATAL)

#define SLOG_WAY_COM        (0x01)
#define SLOG_WAY_FILE       (0X02)
#define SLOG_WAY_WEB        (0x04)
#define SLOG_WAY_ALL        ((SLOG_WAY_COM) | (SLOG_WAY_FILE) | (SLOG_WAY_WEB))

#define LOG_CONTENT_LEN     (20 * 1024)

int DebugPrint(int level, int way, const char *func, int line, const char *format, ...);	

#if 1
#define __DBG(format, arg...)   DebugPrint(0,                SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#define __INFO(format, arg...)  DebugPrint(SLOG_LVL_TRACE,   SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#define __EVENT(format, arg...) DebugPrint(SLOG_LVL_EVENT,   SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#define __WARN(format, arg...)  DebugPrint(SLOG_LVL_WARNING, SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#define __ERR(format, arg...)   DebugPrint(SLOG_LVL_ERROR,   SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#define __FATAL(format, arg...) DebugPrint(SLOG_LVL_FATAL,   SLOG_WAY_ALL, __func__, __LINE__, format, ##arg)
#else
#define __DBG(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#define __INFO(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#define __EVENT(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#define __WARN(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#define __ERR(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#define __FATAL(format, arg...) printf("[%s:%d] " format "\n", __func__, __LINE__, ##arg)
#endif

#define __LOG_ENTER()  	{__INFO("Enter\n");}
#define __LOG_LEAVE()  	{__INFO("Leave\n");}

#define MAX_LOG_FILE_NAME_LEN 512

typedef struct
{
    char log_filename[MAX_LOG_FILE_NAME_LEN];
    int  file_length;
} LOG_INDEX_FILE_ENTRY;

int anj_mw_log_init();

int anj_mw_log_uninit();

LOG_INDEX_FILE_ENTRY *anj_mw_log_filelist_get(struct tm time_start, struct tm time_end, int *count);
void anj_mw_log_cmd_proc(const char *pCmd, int nLength);

#if defined(__cplusplus)
}
#endif

#endif
