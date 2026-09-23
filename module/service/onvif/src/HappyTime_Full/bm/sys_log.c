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

#include "sys_inc.h"
#include "sys_log.h"
#include "anj_mw_log.h"

/***************************************************************************************/

HT_LOG_CTX g_log_ctx = {
	NULL,
	NULL,
	HT_LOG_INFO,// default log level
	0,
	1024,// Maximum file size in KB
	0,
	3,// Maximum file indexes
	1,// Loop write log flag
	0
};

static const char * g_log_level_str[] = 
{
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL"
};

/***************************************************************************************/

void log_get_name(const char * log_fname, char * name, int size)
{
	const char * p;
	p = strrchr(log_fname, '.');
	if (p)
	{
		char fname[256] = {'\0'};
		int len = p - log_fname;
		len = (len <= size ? len : size);
		strncpy(fname, log_fname, len);
		p++;
		if (*p == '\0')
		{
			p = "log";
		}
		snprintf(name, size, "%s-%d.%s", fname, g_log_ctx.cur_idx+1, p);
	}
	else
	{
		snprintf(name, size, "%s-%d.log", log_fname, g_log_ctx.cur_idx+1);
	}
	if (++g_log_ctx.cur_idx >= g_log_ctx.max_idx)
	{
		g_log_ctx.cur_idx = 0;
	}
}

HT_API int log_init(const char * log_fname)
{
	char name[256] = {'\0'};
	log_close();
	if (g_log_ctx.rewind)
	{
		log_get_name(log_fname, name, sizeof(name)-1);
	}
	else
	{
		strncpy(name, log_fname, sizeof(name)-1);
	}
	g_log_ctx.fp = fopen(name, "w+");
	if (g_log_ctx.fp == NULL)
	{
		log_print(HT_LOG_ERR, "log init fopen[%s] failed[%s]\r\n", name, strerror(errno));
		return -1;
	}
	g_log_ctx.cur_size = 0;
	g_log_ctx.mutex = sys_os_create_mutex();
	if (g_log_ctx.mutex == NULL)
	{
		log_print(HT_LOG_ERR, "log init mutex failed[%s]\r\n", strerror(errno));
		return -1;
	}
	if (!g_log_ctx.time_init)
	{
		strncpy(g_log_ctx.name, log_fname, sizeof(g_log_ctx.name)-1);
	}
	return 0;
}

HT_API int log_time_init(const char * fname_prev)
{
	char fpath[256];
	time_t time_now = time(NULL);
	struct tm * st = localtime(&(time_now));
	
	snprintf(fpath, sizeof(fpath), "%s-%04d%02d%02d_%02d%02d%02d.log", fname_prev, st->tm_year+1900, st->tm_mon+1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec);

	g_log_ctx.rewind = 0;
	g_log_ctx.time_init = 1;

	strncpy(g_log_ctx.name, fname_prev, sizeof(g_log_ctx.name)-1);
	
	return log_init(fpath);
}

HT_API int log_reinit(const char * log_fname)
{
	int ret = 0;
	char name[256] = {'\0'};
	sys_os_mutex_enter(g_log_ctx.mutex);
	if (g_log_ctx.fp)
	{
		fclose(g_log_ctx.fp);
		g_log_ctx.fp = NULL;
	}
	if (g_log_ctx.rewind)
	{
		log_get_name(log_fname, name, sizeof(name)-1);
	}
	else
	{
		strncpy(name, log_fname, sizeof(name)-1);
	}
	g_log_ctx.fp = fopen(name, "w+");
	if (g_log_ctx.fp == NULL)
	{
		log_print(HT_LOG_ERR, "log init fopen[%s] failed[%s]\r\n", name, strerror(errno));
		ret = -1;
	}
	g_log_ctx.cur_size = 0;
	if (!g_log_ctx.time_init)
	{
		strncpy(g_log_ctx.name, log_fname, sizeof(g_log_ctx.name)-1);
	}
	sys_os_mutex_leave(g_log_ctx.mutex);
	return ret;
}

HT_API int log_time_reinit(const char * fname_prev)
{
	char fpath[256];
	time_t time_now = time(NULL);
	struct tm * st = localtime(&(time_now));
	snprintf(fpath, sizeof(fpath), "%s-%04d%02d%02d_%02d%02d%02d.log", fname_prev, st->tm_year+1900, st->tm_mon+1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec);
	g_log_ctx.rewind = 0;
	g_log_ctx.time_init = 1;
	strncpy(g_log_ctx.name, fname_prev, sizeof(g_log_ctx.name)-1);
	return log_reinit(fpath);
}

HT_API void log_close()
{
	sys_os_mutex_enter(g_log_ctx.mutex);
	if (g_log_ctx.fp)
	{
		fclose(g_log_ctx.fp);
		g_log_ctx.fp = NULL;
	}
	sys_os_mutex_leave(g_log_ctx.mutex);
	if (g_log_ctx.mutex)
	{
		sys_os_destroy_sig_mutex(g_log_ctx.mutex);
		g_log_ctx.mutex = NULL;
	}
}

void log_switch()
{
	if (g_log_ctx.cur_size >= g_log_ctx.max_size * 1024)
	{
		if (g_log_ctx.time_init)
		{
			log_time_reinit(g_log_ctx.name);
		}
		else
		{
			log_reinit(g_log_ctx.name);
		}
	}
}

static void log_skip_func_prefix(const char ** pfmt, va_list * argptr)
{
	const char * fmt = *pfmt;

	if (fmt == NULL)
	{
		return;
	}
	if (fmt[0] == '%' && fmt[1] == 's' && fmt[2] == ',')
	{
		(void)va_arg(*argptr, const char *);
		fmt += 3;
		if (*fmt == ' ')
		{
			fmt++;
		}
		*pfmt = fmt;
	}
}

static int log_ht_level_to_slog(int level)
{
	switch (level)
	{
	case HT_LOG_WARN:
		return SLOG_LVL_WARNING;
	case HT_LOG_ERR:
		return SLOG_LVL_ERROR;
	case HT_LOG_FATAL:
		return SLOG_LVL_FATAL;
	case HT_LOG_TRC:
	case HT_LOG_DBG:
	case HT_LOG_INFO:
	default:
		return SLOG_LVL_TRACE;
	}
}

int log_print_ex(int level, const char *func, int line, const char *fmt, va_list argptr)
{
	int slen = 0;
	int slog_level;
	va_list arg_copy;
	const char * out_fmt;
	const char * out_func;
	char body[2048];

	if (level < HT_LOG_TRC || level > HT_LOG_FATAL)
	{
		level = HT_LOG_ERR;
	}

	out_func = (func && func[0]) ? func : "-";
	out_fmt = fmt;
	va_copy(arg_copy, argptr);
	log_skip_func_prefix(&out_fmt, &arg_copy);
	slen = vsnprintf(body, sizeof(body), out_fmt, arg_copy);
	va_end(arg_copy);
	if (slen < 0)
	{
		body[0] = '\0';
		slen = 0;
	}

	slog_level = log_ht_level_to_slog(level);
	if (DebugPrint(slog_level, SLOG_WAY_ALL, out_func, line, "%s", body) < 0)
	{
		time_t time_now = time(NULL);
		struct tm * st = localtime(&(time_now));

		fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d][%s][%s:%d]%s",
			st->tm_year+1900, st->tm_mon+1, st->tm_mday, st->tm_hour, st->tm_min, st->tm_sec,
			g_log_level_str[level], out_func, line, body);
		fflush(stderr);
	}

	return slen;
}

HT_API int log_print_impl(int level, const char * func, int line, const char * fmt,...)
{
	if (level < g_log_ctx.level || level > HT_LOG_FATAL)
	{
		return 0;
	}
	else
	{
		int slen;
		va_list argptr;
		va_start(argptr, fmt);
		slen = log_print_ex(level, func, line, fmt, argptr);
		va_end(argptr);
		return slen;
	}
}

static int log_lock_print_ex(const char *fmt, va_list argptr)
{
	int slen;
	if (g_log_ctx.fp == NULL || g_log_ctx.mutex == NULL)
	{
		return 0;
	}
	slen = vfprintf(g_log_ctx.fp, fmt, argptr);
	fflush(g_log_ctx.fp);
	if (slen > 0)
	{
		g_log_ctx.cur_size += slen;
	}
	return slen;
}

HT_API int log_lock_start(const char * fmt,...)
{
	int slen = 0;
	va_list argptr;		
	if (g_log_ctx.fp == NULL || g_log_ctx.mutex == NULL)
	{
		return 0;
	}
	va_start(argptr,fmt);
	sys_os_mutex_enter(g_log_ctx.mutex);
	slen = log_lock_print_ex(fmt,argptr);
	va_end(argptr);
	return slen;
}

HT_API int log_lock_print(const char * fmt,...)
{
	int slen;
	va_list argptr;
	va_start(argptr,fmt);
	slen = log_lock_print_ex(fmt, argptr);
	va_end(argptr);
	return slen;
}

HT_API int log_lock_end(const char * fmt,...)
{
	int slen;
	va_list argptr;
	va_start(argptr,fmt);
	slen = log_lock_print_ex(fmt, argptr);
	va_end(argptr);
	sys_os_mutex_leave(g_log_ctx.mutex);
	log_switch();
	return slen;
}

HT_API void log_set_level(int level)
{
	g_log_ctx.level = level;
}

HT_API int log_get_level()
{
	return g_log_ctx.level;
}

HT_API void log_set_rewind(int rewind)
{
	g_log_ctx.rewind = rewind;
}

HT_API int log_get_rewind()
{
	return g_log_ctx.rewind;
}

HT_API void log_set_max_size(int max_size)
{
	g_log_ctx.max_size = max_size;
}

HT_API int log_get_max_size()
{
	return g_log_ctx.max_size;
}

HT_API void log_set_max_idx(int max_idx)
{
	g_log_ctx.max_idx = max_idx;
}

HT_API int log_get_max_idx()
{
	return g_log_ctx.max_idx;
}



