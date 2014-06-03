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

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_debug.h>
#include <rte_spinlock.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>

#include "eal_private.h"

#define LOG_ELT_SIZE     2048

#define LOG_HISTORY_MP_NAME "log_history"

STAILQ_HEAD(log_history_list, log_history);

/**
 * The structure of a message log in the log history.
 */
struct log_history {
	STAILQ_ENTRY(log_history) next;
	unsigned size;
	char buf[0];
};

static struct rte_mempool *log_history_mp = NULL;
static unsigned log_history_size = 0;
static struct log_history_list log_history;

/* global log structure */
struct rte_logs rte_logs = {
	.type = ~0,
	.level = RTE_LOG_DEBUG,
	.file = NULL,
};

static rte_spinlock_t log_dump_lock = RTE_SPINLOCK_INITIALIZER;
static rte_spinlock_t log_list_lock = RTE_SPINLOCK_INITIALIZER;
static FILE *default_log_stream;
static int history_enabled = 1;

/**
 * This global structure stores some informations about the message
 * that is currently beeing processed by one lcore
 */
struct log_cur_msg {
	uint32_t loglevel; /**< log level - see rte_log.h */
	uint32_t logtype;  /**< log type  - see rte_log.h */
} __rte_cache_aligned;
static struct log_cur_msg log_cur_msg[RTE_MAX_LCORE]; /**< per core log */


/* default logs */

int
rte_log_add_in_history(const char *buf, size_t size)
{
	struct log_history *hist_buf = NULL;
	static const unsigned hist_buf_size = LOG_ELT_SIZE - sizeof(*hist_buf);
	void *obj;

	if (history_enabled == 0)
		return 0;

	rte_spinlock_lock(&log_list_lock);

	/* get a buffer for adding in history */
	if (log_history_size > RTE_LOG_HISTORY) {
		hist_buf = STAILQ_FIRST(&log_history);
		STAILQ_REMOVE_HEAD(&log_history, next);
	}
	else {
		if (rte_mempool_mc_get(log_history_mp, &obj) < 0)
			obj = NULL;
		hist_buf = obj;
	}

	/* no buffer */
	if (hist_buf == NULL) {
		rte_spinlock_unlock(&log_list_lock);
		return -ENOBUFS;
	}

	/* not enough room for msg, buffer go back in mempool */
	if (size >= hist_buf_size) {
		rte_mempool_mp_put(log_history_mp, hist_buf);
		rte_spinlock_unlock(&log_list_lock);
		return -ENOBUFS;
	}

	/* add in history */
	memcpy(hist_buf->buf, buf, size);
	hist_buf->buf[size] = hist_buf->buf[hist_buf_size-1] = '\0';
	hist_buf->size = size;
	STAILQ_INSERT_TAIL(&log_history, hist_buf, next);
	log_history_size++;
	rte_spinlock_unlock(&log_list_lock);

	return 0;
}

void
rte_log_set_history(int enable)
{
	history_enabled = enable;
}

/* Change the stream that will be used by logging system */
int
rte_openlog_stream(FILE *f)
{
	if (f == NULL)
		rte_logs.file = default_log_stream;
	else
		rte_logs.file = f;
	return 0;
}

/* Set global log level */
void
rte_set_log_level(uint32_t level)
{
	rte_logs.level = (uint32_t)level;
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

/* get the current loglevel for the message beeing processed */
int rte_log_cur_msg_loglevel(void)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	return log_cur_msg[lcore_id].loglevel;
}

/* get the current logtype for the message beeing processed */
int rte_log_cur_msg_logtype(void)
{
	unsigned lcore_id;
	lcore_id = rte_lcore_id();
	return log_cur_msg[lcore_id].logtype;
}

/* Dump log history to file */
void
rte_log_dump_history(FILE *out)
{
	struct log_history_list tmp_log_history;
	struct log_history *hist_buf;
	unsigned i;

	/* only one dump at a time */
	rte_spinlock_lock(&log_dump_lock);

	/* save list, and re-init to allow logging during dump */
	rte_spinlock_lock(&log_list_lock);
	tmp_log_history = log_history;
	STAILQ_INIT(&log_history);
	rte_spinlock_unlock(&log_list_lock);

	for (i=0; i<RTE_LOG_HISTORY; i++) {

		/* remove one message from history list */
		hist_buf = STAILQ_FIRST(&tmp_log_history);

		if (hist_buf == NULL)
			break;

		STAILQ_REMOVE_HEAD(&tmp_log_history, next);

		/* write on stdout */
		if (fwrite(hist_buf->buf, hist_buf->size, 1, out) == 0) {
			rte_mempool_mp_put(log_history_mp, hist_buf);
			break;
		}

		/* put back message structure in pool */
		rte_mempool_mp_put(log_history_mp, hist_buf);
	}
	fflush(out);

	rte_spinlock_unlock(&log_dump_lock);
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 */
int
rte_vlog(__attribute__((unused)) uint32_t level,
	 __attribute__((unused)) uint32_t logtype,
	   const char *format, va_list ap)
{
	int ret;
	FILE *f = rte_logs.file;
	unsigned lcore_id;

	/* save loglevel and logtype in a global per-lcore variable */
	lcore_id = rte_lcore_id();
	log_cur_msg[lcore_id].loglevel = level;
	log_cur_msg[lcore_id].logtype = logtype;

	ret = vfprintf(f, format, ap);
	fflush(f);
	return ret;
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
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
 * called by environment-specific log init function to initialize log
 * history
 */
int
rte_eal_common_log_init(FILE *default_log)
{
	STAILQ_INIT(&log_history);

	/* reserve RTE_LOG_HISTORY*2 elements, so we can dump and
	 * keep logging during this time */
	log_history_mp = rte_mempool_create(LOG_HISTORY_MP_NAME, RTE_LOG_HISTORY*2,
				LOG_ELT_SIZE, 0, 0,
				NULL, NULL,
				NULL, NULL,
				SOCKET_ID_ANY, 0);

	if ((log_history_mp == NULL) &&
	    ((log_history_mp = rte_mempool_lookup(LOG_HISTORY_MP_NAME)) == NULL)){
		RTE_LOG(ERR, EAL, "%s(): cannot create log_history mempool\n",
			__func__);
		return -1;
	}

	default_log_stream = default_log;
	rte_openlog_stream(default_log);
	return 0;
}

