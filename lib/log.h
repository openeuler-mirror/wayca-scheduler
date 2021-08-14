/*
 * Copyright (c) 2021 HiSilicon Technologies Co., Ltd.
 * Wayca scheduler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 */

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

/* wayca log level definitions:
 *   INFO: most verbose, prints info(), warn() and err()
 *   WARN: middle level, prints warn() and err()
 *   ERR:  most concise, prints err()
 */
typedef enum {
	WAYCA_SC_LOG_LEVEL_DEFAULT = 2,
	WAYCA_SC_LOG_LEVEL_ERR = 0,
	WAYCA_SC_LOG_LEVEL_WARN = 1,
	WAYCA_SC_LOG_LEVEL_INFO = 2
} WAYCA_SC_LOG_LEVEL;

extern WAYCA_SC_LOG_LEVEL wayca_sc_log_level;

static inline void logTimeStamp(FILE *file)
{
	time_t rawtime;
	struct tm *tm;

	time(&rawtime);
	tm = gmtime(&rawtime);

	/* Probably will never enter this branch */
	if (!tm)
		return;

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

#define WAYCA_SC_LOG_ERR(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_ERR <= wayca_sc_log_level) \
		_log(stderr, false, Red("[Error] ") fmt, ##__VA_ARGS__); \
	} while (0);

#define WAYCA_SC_LOG_ERR_CONN(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_ERR <= wayca_sc_log_level) \
		_log(stderr, true, Red("[Error] ") fmt, ##__VA_ARGS__); \
	} while (0);

#define WAYCA_SC_LOG_WARN(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_WARN <= wayca_sc_log_level) \
		_log(stderr, false, Yellow("[Warning] ") fmt, ##__VA_ARGS__); \
	} while (0);

#define WAYCA_SC_LOG_WARN_CONN(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_WARN <= wayca_sc_log_level) \
		_log(stderr, true, Yellow("[Warning] ") fmt, ##__VA_ARGS__); \
	} while (0);

#define WAYCA_SC_LOG_INFO(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_INFO <= wayca_sc_log_level) \
		_log(stderr, false, fmt, ##__VA_ARGS__); \
	} while (0);

#define WAYCA_SC_LOG_INFO_CONN(fmt, ...) \
	do {if (WAYCA_SC_LOG_LEVEL_INFO <= wayca_sc_log_level) \
		_log(stderr, true, fmt, ##__VA_ARGS__); \
	} while (0);

void wayca_sc_set_log_level(WAYCA_SC_LOG_LEVEL level);
#endif /* LIB_LOG_H */