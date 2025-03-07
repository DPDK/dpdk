/* SPDX-License-Identifier: BSD-3-Clause */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_log.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#include <rte_os_shim.h>
#endif

#include "log_internal.h"
#include "log_private.h"

enum  {
	LOG_COLOR_AUTO = 0,
	LOG_COLOR_NEVER,
	LOG_COLOR_ALWAYS,
} log_color_mode = LOG_COLOR_NEVER;

enum color {
	COLOR_NONE,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_BLUE,
	COLOR_MAGENTA,
	COLOR_CYAN,
	COLOR_WHITE,
	COLOR_BOLD,
	COLOR_CLEAR,
};

enum log_field {
	LOG_FIELD_SUBSYS,
	LOG_FIELD_TIME,
	LOG_FIELD_ALERT,
	LOG_FIELD_ERROR,
	LOG_FIELD_INFO,
};

static const enum color field_colors[] = {
	[LOG_FIELD_SUBSYS] = COLOR_YELLOW,
	[LOG_FIELD_TIME]   = COLOR_GREEN,
	[LOG_FIELD_ALERT]  = COLOR_RED,
	[LOG_FIELD_ERROR]  = COLOR_BOLD,
	[LOG_FIELD_INFO]   = COLOR_NONE,
};

/* If set all colors are bolder */
static bool dark_mode;

/* Standard terminal escape codes for colors and bold */
static const uint8_t color_esc_code[] = {
	[COLOR_RED]	= 31,
	[COLOR_GREEN]	= 32,
	[COLOR_YELLOW]	= 33,
	[COLOR_BLUE]	= 34,
	[COLOR_MAGENTA] = 35,
	[COLOR_CYAN]    = 36,
	[COLOR_WHITE]	= 37,
	[COLOR_BOLD]	= 1,
};

__rte_format_printf(4, 5)
static int
color_snprintf(char *buf, size_t len, enum log_field field,
	       const char *fmt, ...)
{
	enum color color = field_colors[field];
	uint8_t esc = color_esc_code[color];
	va_list args;
	int ret = 0;

	va_start(args, fmt);
	if (esc == 0) {
		ret = vsnprintf(buf, len, fmt, args);
	} else {
		ret = snprintf(buf, len,
			       dark_mode ? "\033[1;%um" : "\033[%um", esc);
		ret += vsnprintf(buf + ret, len - ret, fmt, args);
		ret += snprintf(buf + ret,  len - ret, "%s", "\033[0m");
	}
	va_end(args);

	return ret;
}

/*
 * Controls whether color is enabled:
 * modes are:
 *   always - enable color output regardless
 *   auto - enable if stderr is a terminal
 *   never - color output is disabled.
 */
RTE_EXPORT_INTERNAL_SYMBOL(eal_log_color)
int
eal_log_color(const char *mode)
{
	if (mode == NULL || strcmp(mode, "always") == 0)
		log_color_mode = LOG_COLOR_ALWAYS;
	else if (strcmp(mode, "never") == 0)
		log_color_mode = LOG_COLOR_NEVER;
	else if (strcmp(mode, "auto") == 0)
		log_color_mode = LOG_COLOR_AUTO;
	else
		return -1;

	return 0;
}

bool
log_color_enabled(bool is_terminal)
{
	char *env, *sep;

	/* Set dark mode using the defacto heuristics used by other programs */
	env = getenv("COLORFGBG");
	if (env) {
		sep = strrchr(env, ';');
		if (sep &&
		    ((sep[1] >= '0' && sep[1] <= '6') || sep[1] == '8') &&
		    sep[2] == '\0')
			dark_mode = true;
	}

	if (log_color_mode == LOG_COLOR_ALWAYS)
		return true;
	else if (log_color_mode == LOG_COLOR_AUTO)
		return is_terminal;
	else
		return false;
}

/* Look ast the current message level to determine color of field */
static enum log_field
color_msg_field(void)
{
	const int level = rte_log_cur_msg_loglevel();

	if (level <= 0 || level >= (int)RTE_LOG_INFO)
		return LOG_FIELD_INFO;
	else if (level >= (int)RTE_LOG_ERR)
		return LOG_FIELD_ERROR;
	else
		return LOG_FIELD_ALERT;
}

__rte_format_printf(3, 0)
static int
color_fmt_msg(char *out, size_t len, const char *format, va_list ap)
{
	enum log_field field = color_msg_field();
	char buf[LINE_MAX];
	int ret = 0;

	/* format raw message */
	vsnprintf(buf, sizeof(buf), format, ap);
	const char *msg = buf;

	/*
	 * use convention that first part of message (up to the ':' character)
	 * is the subsystem id and should be highlighted.
	 */
	const char *cp = strchr(msg, ':');
	if (cp) {
		/* print first part in yellow */
		ret = color_snprintf(out, len, LOG_FIELD_SUBSYS,
				     "%.*s", (int)(cp - msg + 1), msg);
		/* skip the first part */
		msg = cp + 1;
	}

	ret += color_snprintf(out + ret, len - ret, field, "%s", msg);
	return ret;
}

__rte_format_printf(2, 0)
int
color_print(FILE *f, const char *format, va_list ap)
{
	char out[LINE_MAX];

	/* format raw message */
	int ret = color_fmt_msg(out, sizeof(out), format, ap);
	if (fputs(out, f) < 0)
		return -1;

	return ret;
}

__rte_format_printf(2, 0)
int
color_print_with_timestamp(FILE *f, const char *format, va_list ap)
{
	char out[LINE_MAX];
	char tsbuf[128];
	int ret = 0;

	if (log_timestamp(tsbuf, sizeof(tsbuf)) > 0)
		ret = color_snprintf(out, sizeof(out),
				     LOG_FIELD_TIME, "[%s] ", tsbuf);

	ret += color_fmt_msg(out + ret, sizeof(out) - ret, format, ap);
	if (fputs(out, f) < 0)
		return -1;

	return ret;
}
