/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_log.h>

#include "log_internal.h"
#include "log_private.h"

static int log_facility;

/*
 * Usable list of facilities
 * Skip kern, mark, and security
 */
static const struct {
	const char *name;
	int value;
} facilitys[] = {
	{ "auth", LOG_AUTH },
	{ "cron", LOG_CRON },
	{ "daemon", LOG_DAEMON },
	{ "ftp", LOG_FTP },
	{ "kern", LOG_KERN },
	{ "lpr", LOG_LPR },
	{ "mail", LOG_MAIL },
	{ "news", LOG_NEWS },
	{ "syslog", LOG_SYSLOG },
	{ "user", LOG_USER },
	{ "uucp", LOG_UUCP },
	{ "local0", LOG_LOCAL0 },
	{ "local1", LOG_LOCAL1 },
	{ "local2", LOG_LOCAL2 },
	{ "local3", LOG_LOCAL3 },
	{ "local4", LOG_LOCAL4 },
	{ "local5", LOG_LOCAL5 },
	{ "local6", LOG_LOCAL6 },
	{ "local7", LOG_LOCAL7 },
};

RTE_EXPORT_INTERNAL_SYMBOL(eal_log_syslog)
int
eal_log_syslog(const char *name)
{
	unsigned int i;

	if (name == NULL) {
		log_facility = LOG_DAEMON;
		return 0;
	}

	for (i = 0; i < RTE_DIM(facilitys); i++) {
		if (!strcmp(name, facilitys[i].name)) {
			log_facility = facilitys[i].value;
			return 0;
		}
	}
	return -1;
}

/* syslog is enabled if facility is set */
bool
log_syslog_enabled(void)
{
	return log_facility != 0; /* LOG_KERN is 0 */
}

/*
 * default log function
 */
static ssize_t
log_syslog_write(__rte_unused void *c, const char *buf, size_t size)
{
	/* Syslog error levels are from 0 to 7, so subtract 1 to convert */
	syslog(rte_log_cur_msg_loglevel() - 1, "%.*s", (int)size, buf);

	return size;
}

static int
log_syslog_close(__rte_unused void *c)
{
	closelog();
	return 0;
}

static cookie_io_functions_t log_syslog_func = {
	.write = log_syslog_write,
	.close = log_syslog_close,
};


FILE *
log_syslog_open(const char *id)
{
	int option = LOG_CONS | LOG_NDELAY | LOG_PID | LOG_PERROR;

	openlog(id, option, log_facility);

	/* redirect other log messages to syslog as well */
	return fopencookie(NULL, "w", log_syslog_func);
}
