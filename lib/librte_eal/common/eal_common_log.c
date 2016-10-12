/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>

#include <rte_log.h>
#include <rte_per_lcore.h>

#include "eal_private.h"

/* global log structure */
struct rte_logs rte_logs = {
	.type = ~0,
	.level = RTE_LOG_DEBUG,
	.file = NULL,
};

/* Stream to use for logging if rte_logs.file is NULL */
static FILE *default_log_stream;

/**
 * This global structure stores some informations about the message
 * that is currently being processed by one lcore
 */
struct log_cur_msg {
	uint32_t loglevel; /**< log level - see rte_log.h */
	uint32_t logtype;  /**< log type  - see rte_log.h */
};

 /* per core log */
static RTE_DEFINE_PER_LCORE(struct log_cur_msg, log_cur_msg);

/* default logs */

/* Change the stream that will be used by logging system */
int
rte_openlog_stream(FILE *f)
{
	rte_logs.file = f;
	return 0;
}

/* Set global log level */
void
rte_set_log_level(uint32_t level)
{
	rte_logs.level = (uint32_t)level;
}

/* Get global log level */
uint32_t
rte_get_log_level(void)
{
	return rte_logs.level;
}

/* Set global log type */
void
rte_set_log_type(uint32_t type, int enable)
{
	if (enable)
		rte_logs.type |= type;
	else
		rte_logs.type &= (~type);
}

/* Get global log type */
uint32_t
rte_get_log_type(void)
{
	return rte_logs.type;
}

/* get the current loglevel for the message beeing processed */
int rte_log_cur_msg_loglevel(void)
{
	return RTE_PER_LCORE(log_cur_msg).loglevel;
}

/* get the current logtype for the message beeing processed */
int rte_log_cur_msg_logtype(void)
{
	return RTE_PER_LCORE(log_cur_msg).logtype;
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 */
int
rte_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap)
{
	int ret;
	FILE *f = rte_logs.file;
	if (f == NULL) {
		f = default_log_stream;
		if (f == NULL) {
			/*
			 * Grab the current value of stderr here, rather than
			 * just initializing default_log_stream to stderr. This
			 * ensures that we will always use the current value
			 * of stderr, even if the application closes and
			 * reopens it.
			 */
			f = stderr;
		}
	}

	if ((level > rte_logs.level) || !(logtype & rte_logs.type))
		return 0;

	/* save loglevel and logtype in a global per-lcore variable */
	RTE_PER_LCORE(log_cur_msg).loglevel = level;
	RTE_PER_LCORE(log_cur_msg).logtype = logtype;

	ret = vfprintf(f, format, ap);
	fflush(f);
	return ret;
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 * No need to check level here, done by rte_vlog().
 */
int
rte_log(uint32_t level, uint32_t logtype, const char *format, ...)
{
	va_list ap;
	int ret;

	va_start(ap, format);
	ret = rte_vlog(level, logtype, format, ap);
	va_end(ap);
	return ret;
}

/*
 * Called by environment-specific initialization functions.
 */
void
eal_log_set_default(FILE *default_log)
{
	default_log_stream = default_log;

#if RTE_LOG_LEVEL >= RTE_LOG_DEBUG
	RTE_LOG(NOTICE, EAL, "Debug logs available - lower performance\n");
#endif
}
