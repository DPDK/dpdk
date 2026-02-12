/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef LOG_PRIVATE_H
#define LOG_PRIVATE_H

/* Defined in limits.h on Linux */
#ifndef LINE_MAX
#define LINE_MAX	2048 /* _POSIX2_LINE_MAX */
#endif

#ifdef RTE_EXEC_ENV_WINDOWS
static inline bool
log_syslog_enabled(void)
{
	return false;
}
static inline FILE *
log_syslog_open(const char *id __rte_unused)
{
	return NULL;
}
#else
bool log_syslog_enabled(void);
FILE *log_syslog_open(const char *id);
#endif

#ifdef RTE_EXEC_ENV_LINUX
bool log_journal_enabled(void);
FILE *log_journal_open(const char *id);
#else
static inline bool
log_journal_enabled(void)
{
	return false;
}
static inline FILE *
log_journal_open(const char *id __rte_unused)
{
	return NULL;
}
#endif /* !RTE_EXEC_ENV_LINUX */

bool log_timestamp_enabled(void);
ssize_t log_timestamp(char *tsbuf, size_t tsbuflen);

__rte_format_printf(2, 0)
int log_print_with_timestamp(FILE *f, const char *format, va_list ap);

bool log_color_enabled(bool is_tty);

__rte_format_printf(2, 0)
int color_print(FILE *f, const char *format, va_list ap);

__rte_format_printf(2, 0)
int color_print_with_timestamp(FILE *f, const char *format, va_list ap);

#endif /* LOG_PRIVATE_H */
