/* SPDX-License-Identifier: BSD-3-Clause */

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_stdatomic.h>
#include <rte_time.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#include <rte_os_shim.h>
#endif

#include "log_internal.h"
#include "log_private.h"

#ifndef NS_PER_SEC
#define NS_PER_SEC 1E9
#endif

static enum {
	LOG_TIMESTAMP_NONE = 0,
	LOG_TIMESTAMP_TIME,	/* time since start */
	LOG_TIMESTAMP_DELTA,	/* time since last message */
	LOG_TIMESTAMP_RELTIME,  /* relative time since last message */
	LOG_TIMESTAMP_CTIME,	/* Unix standard time format */
	LOG_TIMESTAMP_ISO,	/* ISO8601 time format */
} log_time_format;

static struct {
	struct timespec started;   /* when log was initialized */
	RTE_ATOMIC(uint64_t) last_monotonic;
	RTE_ATOMIC(uint64_t) last_realtime;
} log_time;

/* Set the log timestamp format */
RTE_EXPORT_INTERNAL_SYMBOL(eal_log_timestamp)
int
eal_log_timestamp(const char *str)
{
	if (str == NULL)
		log_time_format = LOG_TIMESTAMP_TIME;
	else if (strcmp(str, "notime") == 0)
		log_time_format = LOG_TIMESTAMP_NONE;
	else if (strcmp(str, "reltime") == 0)
		log_time_format = LOG_TIMESTAMP_RELTIME;
	else if (strcmp(str, "delta") == 0)
		log_time_format = LOG_TIMESTAMP_DELTA;
	else if (strcmp(str, "ctime") == 0)
		log_time_format =  LOG_TIMESTAMP_CTIME;
	else if (strcmp(str, "iso") == 0)
		log_time_format = LOG_TIMESTAMP_ISO;
	else
		return -1;

	return 0;
}

bool
log_timestamp_enabled(void)
{
	return log_time_format != LOG_TIMESTAMP_NONE;
}

/* Subtract two timespec values and handle wraparound */
static struct timespec
timespec_sub(const struct timespec *t0, const struct timespec *t1)
{
	struct timespec ts;

	ts.tv_sec = t0->tv_sec - t1->tv_sec;
	ts.tv_nsec = t0->tv_nsec - t1->tv_nsec;
	if (ts.tv_nsec < 0) {
		ts.tv_sec--;
		ts.tv_nsec += 1000000000L;
	}
	return ts;
}

/*
 * Format current timespec into ISO8601 format.
 * Surprisingly, can't just use strftime() for this;
 * since want microseconds and the timezone offset format differs.
 */
static ssize_t
format_iso8601(char *tsbuf, size_t tsbuflen, const struct timespec *now)
{
	struct tm *tm, tbuf;
	char dbuf[64]; /* "2024-05-01T22:11:00" */
	char zbuf[16] = { }; /* "+0800" */

	tm = localtime_r(&now->tv_sec, &tbuf);

	/* make "2024-05-01T22:11:00,123456+0100" */
	if (strftime(dbuf, sizeof(dbuf), "%Y-%m-%dT%H:%M:%S", tm) == 0)
		return 0;

	/* convert timezone to +hhmm */
	if (strftime(zbuf, sizeof(zbuf), "%z", tm) == 0)
		return 0;

	/* the result for strftime is "+hhmm" but ISO wants "+hh:mm" */
	return snprintf(tsbuf, tsbuflen, "%s,%06lu%.3s:%.2s",
			dbuf, now->tv_nsec / 1000u,
			zbuf, zbuf + 3);
}

/*
 * Format a timestamp which shows time between messages.
 */
static ssize_t
format_delta(char *tsbuf, size_t tsbuflen, const struct timespec *now)
{
	struct timespec delta;
	uint64_t ns = rte_timespec_to_ns(now);
	uint64_t previous;

	previous = rte_atomic_exchange_explicit(&log_time.last_monotonic,
						ns, rte_memory_order_seq_cst);
	delta = rte_ns_to_timespec(ns - previous);

	return snprintf(tsbuf, tsbuflen, "<%6lu.%06lu>",
			(unsigned long)delta.tv_sec,
			(unsigned long)delta.tv_nsec / 1000u);
}

/*
 * Make a timestamp where if the minute, hour or day has
 * changed from the last message, then print abbreviated
 * "Month day hour:minute" format.
 * Otherwise print delta from last printed message as +sec.usec
 */
static ssize_t
format_reltime(char *tsbuf, size_t tsbuflen, const struct timespec *now)
{
	struct tm *tm, *last_tm, tbuf1, tbuf2;
	time_t last_sec;
	uint64_t ns = rte_timespec_to_ns(now);
	uint64_t previous;

	tm = localtime_r(&now->tv_sec, &tbuf1);

	previous = rte_atomic_exchange_explicit(&log_time.last_realtime,
						ns, rte_memory_order_seq_cst);
	last_sec = previous / NS_PER_SEC;
	last_tm = localtime_r(&last_sec, &tbuf2);
	if (tm->tm_min == last_tm->tm_min &&
	    tm->tm_hour == last_tm->tm_hour &&
	    tm->tm_yday == last_tm->tm_yday) {
		struct timespec elapsed;

		elapsed = rte_ns_to_timespec(ns - previous);

		return snprintf(tsbuf, tsbuflen, "+%3lu.%06lu",
				(unsigned long)elapsed.tv_sec,
				(unsigned long)elapsed.tv_nsec / 1000u);
	} else {
		return strftime(tsbuf, tsbuflen, "%b%d %H:%M", tm);
	}
}

/* Format up a timestamp based on current format */
ssize_t
log_timestamp(char *tsbuf, size_t tsbuflen)
{
	struct timespec now, delta;

	switch (log_time_format) {
	case LOG_TIMESTAMP_NONE:
		return 0;

	case LOG_TIMESTAMP_TIME:
		if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
			return 0;

		delta = timespec_sub(&now, &log_time.started);

		return snprintf(tsbuf, tsbuflen, "%6lu.%06lu",
				(unsigned long)delta.tv_sec,
				(unsigned long)delta.tv_nsec / 1000u);

	case LOG_TIMESTAMP_DELTA:
		if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
			return 0;

		return format_delta(tsbuf, tsbuflen, &now);

	case LOG_TIMESTAMP_RELTIME:
		if (clock_gettime(CLOCK_REALTIME, &now) < 0)
			return 0;

		return format_reltime(tsbuf, tsbuflen, &now);

	case LOG_TIMESTAMP_CTIME:
		if (clock_gettime(CLOCK_REALTIME, &now) < 0)
			return 0;

		/* trncate to remove newline from ctime result */
		return snprintf(tsbuf, tsbuflen, "%.24s", ctime(&now.tv_sec));

	case LOG_TIMESTAMP_ISO:
		if (clock_gettime(CLOCK_REALTIME, &now) < 0)
			return 0;

		return format_iso8601(tsbuf, tsbuflen, &now);
	}

	return 0;
}

/* print timestamp before message */
int
log_print_with_timestamp(FILE *f, const char *format, va_list ap)
{
	char tsbuf[128];
	char msgbuf[LINE_MAX];

	if (log_timestamp(tsbuf, sizeof(tsbuf)) > 0) {
		vsnprintf(msgbuf, sizeof(msgbuf), format, ap);
		return fprintf(f, "[%s] %s", tsbuf, msgbuf);
	}

	/* fall back when timestamp is unavailable */
	return vfprintf(f, format, ap);
}

RTE_INIT_PRIO(log_timestamp_init, LOG)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	log_time.started = now;
	rte_atomic_store_explicit(&log_time.last_monotonic, rte_timespec_to_ns(&now),
				  rte_memory_order_seq_cst);
}
