#ifndef LIB_LOG_H
#define LIB_LOG_H

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#define Reset 		"\033[0m"

#define Black(str) 		"\033[30m" str Reset
#define Red(str)		"\033[31m" str Reset
#define Green(str) 		"\033[32m" str Reset
#define Yellow(str) 		"\033[33m" str Reset
#define Blue(str) 		"\033[34m" str Reset
#define Magenta(str) 		"\033[35m" str Reset
#define Cyan(str) 		"\033[36m" str Reset
#define White(str) 		"\033[37m" str Reset

static inline void logTimeStamp(FILE *file)
{
	time_t rawtime;
	struct tm *tm;

	time(&rawtime);
	tm = gmtime(&rawtime);

	fprintf(file, "[%04d-%02d-%02d %02d:%02d:%02d]",
		tm->tm_year + 1900, tm->tm_mon, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static inline void _log(FILE *file, bool conn, char *fmt, ...)
{
	va_list args;

	if (!conn)
		logTimeStamp(file);

	va_start(args, fmt);
	vfprintf(file, fmt, args);
	va_end(args);
}

#define info(fmt, ...)	\
	_log(stdout, false, "[Info] " fmt, __VA_ARGS__)
#define err(fmt, ...)	\
	_log(stderr, false, Red("[Error] ") fmt, __VA_ARGS__)
#define warn(fmt, ...)	\
	_log(stdout, false, Yellow("[Warn] ") fmt, __VA_ARGS__)

#define info_conn(fmt, ...)	\
	_log(stdout, true, "[Info] " fmt, __VA_ARGS__)
#define err_conn(fmt, ...)	\
	_log(stderr, true, Red("[Error] ") fmt, __VA_ARGS__)
#define warn_conn(fmt, ...)	\
	_log(stdout, true, Yellow("[Warn] ") fmt, __VA_ARGS__)

#endif /* LIB_LOG_H */
