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

#endif /* LOG_PRIVATE_H */
