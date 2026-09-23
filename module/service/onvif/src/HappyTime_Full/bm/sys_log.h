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

#ifndef	__H_SYS_LOG_H__
#define	__H_SYS_LOG_H__

// log level

#define HT_LOG_TRC     0        
#define HT_LOG_DBG     1
#define HT_LOG_INFO    2
#define HT_LOG_WARN    3
#define HT_LOG_ERR     4
#define HT_LOG_FATAL   5

typedef struct
{
	FILE  * fp;         // Log file pointer
	void  * mutex;      // read-write lock
	int     level;      // Log level
	uint32  cur_size;   // The current write length of the log
	uint32  max_size;   // Maximum file size in KB
	int     cur_idx;    // Current log file index
	int     max_idx;    // Maximum file indexes
	int     rewind;     // Loop write log flag
	int     time_init;  // Initialize log file names using system time
	char    name[256];  // Log file name
} HT_LOG_CTX;

#ifdef __cplusplus
extern "C" {
#endif

HT_API int  log_init(const char * log_fname);
HT_API int  log_time_init(const char * fname_prev);
HT_API int  log_reinit(const char * log_fname);
HT_API int  log_time_reinit(const char * fname_prev);
HT_API void log_close();
HT_API void log_set_level(int level);
HT_API int  log_get_level();
HT_API void log_set_rewind(int rewind);
HT_API int  log_get_rewind();
HT_API void log_set_max_size(int max_size);
HT_API int  log_get_max_size();
HT_API void log_set_max_idx(int max_idx);
HT_API int  log_get_max_idx();
HT_API int  log_lock_start(const char * fmt,...);
HT_API int  log_lock_print(const char * fmt,...);
HT_API int  log_lock_end(const char * fmt,...);

HT_API int  log_print_impl(int level, const char * func, int line, const char * fmt,...);
#define log_print(level, fmt, ...) log_print_impl((level), __func__, __LINE__, (fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif



