/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <regex.h>
#include <fnmatch.h>
#include <sys/queue.h>
#include <unistd.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_per_lcore.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#include <rte_os_shim.h>
#endif

#include "log_internal.h"
#include "log_private.h"

struct rte_log_dynamic_type {
	const char *name;
	uint32_t loglevel;
};

/* Note: same as vfprintf() */
typedef int (*log_print_t)(FILE *f, const char *fmt, va_list ap);

/** The rte_log structure. */
static struct rte_logs {
	uint32_t type;  /**< Bitfield with enabled logs. */
	uint32_t level; /**< Log level. */
	FILE *file;     /**< Output file set by rte_openlog_stream, or NULL. */
	bool is_internal_file;
	log_print_t print_func;
	size_t dynamic_types_len;
	struct rte_log_dynamic_type *dynamic_types;
} rte_logs = {
	.type = UINT32_MAX,
	.level = RTE_LOG_DEBUG,
	.print_func = vfprintf,
};

struct rte_eal_opt_loglevel {
	/** Next list entry */
	TAILQ_ENTRY(rte_eal_opt_loglevel) next;
	/** Compiled regular expression obtained from the option */
	regex_t re_match;
	/** Globbing pattern option */
	char *pattern;
	/** Log level value obtained from the option */
	uint32_t level;
};

TAILQ_HEAD(rte_eal_opt_loglevel_list, rte_eal_opt_loglevel);

/** List of valid EAL log level options */
static struct rte_eal_opt_loglevel_list opt_loglevel_list =
	TAILQ_HEAD_INITIALIZER(opt_loglevel_list);

/**
 * This global structure stores some information about the message
 * that is currently being processed by one lcore
 */
struct log_cur_msg {
	uint32_t loglevel; /**< log level - see rte_log.h */
	uint32_t logtype;  /**< log type  - see rte_log.h */
};

 /* per core log */
static RTE_DEFINE_PER_LCORE(struct log_cur_msg, log_cur_msg);

/* Change the stream that will be used by logging system */
RTE_EXPORT_SYMBOL(rte_openlog_stream)
int
rte_openlog_stream(FILE *f)
{
	if (rte_logs.is_internal_file && rte_logs.file != NULL)
		fclose(rte_logs.file);
	rte_logs.file = f;
	rte_logs.print_func = vfprintf;
	rte_logs.is_internal_file = false;
	return 0;
}

RTE_EXPORT_SYMBOL(rte_log_get_stream)
FILE *
rte_log_get_stream(void)
{
	FILE *f = rte_logs.file;

	return (f == NULL) ? stderr : f;
}

/* Set global log level */
RTE_EXPORT_SYMBOL(rte_log_set_global_level)
void
rte_log_set_global_level(uint32_t level)
{
	rte_logs.level = (uint32_t)level;
}

/* Get global log level */
RTE_EXPORT_SYMBOL(rte_log_get_global_level)
uint32_t
rte_log_get_global_level(void)
{
	return rte_logs.level;
}

RTE_EXPORT_SYMBOL(rte_log_get_level)
int
rte_log_get_level(uint32_t type)
{
	if (type >= rte_logs.dynamic_types_len)
		return -1;

	return rte_logs.dynamic_types[type].loglevel;
}

RTE_EXPORT_SYMBOL(rte_log_can_log)
bool
rte_log_can_log(uint32_t logtype, uint32_t level)
{
	int log_level;

	if (level > rte_log_get_global_level())
		return false;

	log_level = rte_log_get_level(logtype);
	if (log_level < 0)
		return false;

	if (level > (uint32_t)log_level)
		return false;

	return true;
}

static void
logtype_set_level(uint32_t type, uint32_t level)
{
	uint32_t current = rte_logs.dynamic_types[type].loglevel;

	if (current != level) {
		rte_logs.dynamic_types[type].loglevel = level;
		RTE_LOG(DEBUG, EAL, "%s log level changed from %s to %s\n",
			rte_logs.dynamic_types[type].name == NULL ?
				"" : rte_logs.dynamic_types[type].name,
			eal_log_level2str(current),
			eal_log_level2str(level));
	}
}

RTE_EXPORT_SYMBOL(rte_log_set_level)
int
rte_log_set_level(uint32_t type, uint32_t level)
{
	if (type >= rte_logs.dynamic_types_len)
		return -1;
	if (level > RTE_LOG_MAX)
		return -1;

	logtype_set_level(type, level);

	return 0;
}

/* set log level by regular expression */
RTE_EXPORT_SYMBOL(rte_log_set_level_regexp)
int
rte_log_set_level_regexp(const char *regex, uint32_t level)
{
	regex_t r;
	size_t i;

	if (level > RTE_LOG_MAX)
		return -1;

	if (regcomp(&r, regex, 0) != 0)
		return -1;

	for (i = 0; i < rte_logs.dynamic_types_len; i++) {
		if (rte_logs.dynamic_types[i].name == NULL)
			continue;
		if (regexec(&r, rte_logs.dynamic_types[i].name, 0,
				NULL, 0) == 0)
			logtype_set_level(i, level);
	}

	regfree(&r);

	return 0;
}

/*
 * Save the type string and the loglevel for later dynamic
 * logtypes which may register later.
 */
static int
log_save_level(uint32_t priority, const char *regex, const char *pattern)
{
	struct rte_eal_opt_loglevel *opt_ll = NULL;

	opt_ll = malloc(sizeof(*opt_ll));
	if (opt_ll == NULL)
		goto fail;

	opt_ll->level = priority;

	if (regex) {
		opt_ll->pattern = NULL;
		if (regcomp(&opt_ll->re_match, regex, 0) != 0)
			goto fail;
	} else if (pattern) {
		opt_ll->pattern = strdup(pattern);
		if (opt_ll->pattern == NULL)
			goto fail;
	} else
		goto fail;

	TAILQ_INSERT_HEAD(&opt_loglevel_list, opt_ll, next);
	return 0;
fail:
	free(opt_ll);
	return -1;
}

RTE_EXPORT_INTERNAL_SYMBOL(eal_log_save_regexp)
int
eal_log_save_regexp(const char *regex, uint32_t level)
{
	return log_save_level(level, regex, NULL);
}

/* set log level based on globbing pattern */
RTE_EXPORT_SYMBOL(rte_log_set_level_pattern)
int
rte_log_set_level_pattern(const char *pattern, uint32_t level)
{
	size_t i;

	if (level > RTE_LOG_MAX)
		return -1;

	for (i = 0; i < rte_logs.dynamic_types_len; i++) {
		if (rte_logs.dynamic_types[i].name == NULL)
			continue;

		if (fnmatch(pattern, rte_logs.dynamic_types[i].name, 0) == 0)
			logtype_set_level(i, level);
	}

	return 0;
}

RTE_EXPORT_INTERNAL_SYMBOL(eal_log_save_pattern)
int
eal_log_save_pattern(const char *pattern, uint32_t level)
{
	return log_save_level(level, NULL, pattern);
}

/* get the current loglevel for the message being processed */
RTE_EXPORT_SYMBOL(rte_log_cur_msg_loglevel)
int rte_log_cur_msg_loglevel(void)
{
	return RTE_PER_LCORE(log_cur_msg).loglevel;
}

/* get the current logtype for the message being processed */
RTE_EXPORT_SYMBOL(rte_log_cur_msg_logtype)
int rte_log_cur_msg_logtype(void)
{
	return RTE_PER_LCORE(log_cur_msg).logtype;
}

static int
log_lookup(const char *name)
{
	size_t i;

	for (i = 0; i < rte_logs.dynamic_types_len; i++) {
		if (rte_logs.dynamic_types[i].name == NULL)
			continue;
		if (strcmp(name, rte_logs.dynamic_types[i].name) == 0)
			return i;
	}

	return -1;
}

static int
log_register(const char *name, uint32_t level)
{
	struct rte_log_dynamic_type *new_dynamic_types;
	int id;

	id = log_lookup(name);
	if (id >= 0)
		return id;

	new_dynamic_types = realloc(rte_logs.dynamic_types,
		sizeof(struct rte_log_dynamic_type) *
		(rte_logs.dynamic_types_len + 1));
	if (new_dynamic_types == NULL)
		return -ENOMEM;
	rte_logs.dynamic_types = new_dynamic_types;

	id = rte_logs.dynamic_types_len;
	memset(&rte_logs.dynamic_types[id], 0,
		sizeof(rte_logs.dynamic_types[id]));
	rte_logs.dynamic_types[id].name = strdup(name);
	if (rte_logs.dynamic_types[id].name == NULL)
		return -ENOMEM;
	logtype_set_level(id, level);

	rte_logs.dynamic_types_len++;

	return id;
}

/* register an extended log type */
RTE_EXPORT_SYMBOL(rte_log_register)
int
rte_log_register(const char *name)
{
	return log_register(name, RTE_LOG_INFO);
}

/* Register an extended log type and try to pick its level from EAL options */
RTE_EXPORT_SYMBOL(rte_log_register_type_and_pick_level)
int
rte_log_register_type_and_pick_level(const char *name, uint32_t level_def)
{
	struct rte_eal_opt_loglevel *opt_ll;
	uint32_t level = level_def;

	TAILQ_FOREACH(opt_ll, &opt_loglevel_list, next) {
		if (opt_ll->level > RTE_LOG_MAX)
			continue;

		if (opt_ll->pattern) {
			if (fnmatch(opt_ll->pattern, name, 0) == 0)
				level = opt_ll->level;
		} else {
			if (regexec(&opt_ll->re_match, name, 0, NULL, 0) == 0)
				level = opt_ll->level;
		}
	}

	return log_register(name, level);
}

struct logtype {
	uint32_t log_id;
	const char *logtype;
};

static const struct logtype logtype_strings[] = {
	{RTE_LOGTYPE_EAL,        "lib.eal"},

	{RTE_LOGTYPE_USER1,      "user1"},
	{RTE_LOGTYPE_USER2,      "user2"},
	{RTE_LOGTYPE_USER3,      "user3"},
	{RTE_LOGTYPE_USER4,      "user4"},
	{RTE_LOGTYPE_USER5,      "user5"},
	{RTE_LOGTYPE_USER6,      "user6"},
	{RTE_LOGTYPE_USER7,      "user7"},
	{RTE_LOGTYPE_USER8,      "user8"}
};

/* Logging should be first initializer (before drivers and bus) */
RTE_INIT_PRIO(log_init, LOG)
{
	uint32_t i;

	rte_log_set_global_level(RTE_LOG_DEBUG);

	rte_logs.dynamic_types = calloc(RTE_LOGTYPE_FIRST_EXT_ID,
		sizeof(struct rte_log_dynamic_type));
	if (rte_logs.dynamic_types == NULL)
		return;

	/* register legacy log types */
	for (i = 0; i < RTE_DIM(logtype_strings); i++) {
		rte_logs.dynamic_types[logtype_strings[i].log_id].name =
			strdup(logtype_strings[i].logtype);
		logtype_set_level(logtype_strings[i].log_id, RTE_LOG_INFO);
	}

	rte_logs.dynamic_types_len = RTE_LOGTYPE_FIRST_EXT_ID;
}

RTE_EXPORT_INTERNAL_SYMBOL(eal_log_level2str)
const char *
eal_log_level2str(uint32_t level)
{
	switch (level) {
	case 0: return "disabled";
	case RTE_LOG_EMERG: return "emergency";
	case RTE_LOG_ALERT: return "alert";
	case RTE_LOG_CRIT: return "critical";
	case RTE_LOG_ERR: return "error";
	case RTE_LOG_WARNING: return "warning";
	case RTE_LOG_NOTICE: return "notice";
	case RTE_LOG_INFO: return "info";
	case RTE_LOG_DEBUG: return "debug";
	default: return "unknown";
	}
}

static int
log_type_compare(const void *a, const void *b)
{
	const struct rte_log_dynamic_type *type_a = a;
	const struct rte_log_dynamic_type *type_b = b;

	if (type_a->name == NULL && type_b->name == NULL)
		return 0;
	if (type_a->name == NULL)
		return -1;
	if (type_b->name == NULL)
		return 1;
	return strcmp(type_a->name, type_b->name);
}

/* Dump name of each logtype, one per line. */
RTE_EXPORT_SYMBOL(rte_log_list_types)
void
rte_log_list_types(FILE *out, const char *prefix)
{
	struct rte_log_dynamic_type *sorted_types;
	const size_t type_size = sizeof(rte_logs.dynamic_types[0]);
	const size_t type_count = rte_logs.dynamic_types_len;
	const size_t total_size = type_size * type_count;
	size_t type;

	sorted_types = malloc(total_size);
	if (sorted_types == NULL) {
		/* no sorting - unlikely */
		sorted_types = rte_logs.dynamic_types;
	} else {
		memcpy(sorted_types, rte_logs.dynamic_types, total_size);
		qsort(sorted_types, type_count, type_size, log_type_compare);
	}

	for (type = 0; type < type_count; ++type) {
		if (sorted_types[type].name == NULL)
			continue;
		fprintf(out, "%s%s\n", prefix, sorted_types[type].name);
	}

	if (sorted_types != rte_logs.dynamic_types)
		free(sorted_types);
}

/* dump global level and registered log types */
RTE_EXPORT_SYMBOL(rte_log_dump)
void
rte_log_dump(FILE *f)
{
	size_t i;

	fprintf(f, "global log level is %s\n",
		eal_log_level2str(rte_log_get_global_level()));

	for (i = 0; i < rte_logs.dynamic_types_len; i++) {
		if (rte_logs.dynamic_types[i].name == NULL)
			continue;
		fprintf(f, "id %zu: %s, level is %s\n",
			i, rte_logs.dynamic_types[i].name,
			eal_log_level2str(rte_logs.dynamic_types[i].loglevel));
	}
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 */
RTE_EXPORT_SYMBOL(rte_vlog)
int
rte_vlog(uint32_t level, uint32_t logtype, const char *format, va_list ap)
{
	FILE *f = rte_log_get_stream();
	int ret;

	if (logtype >= rte_logs.dynamic_types_len)
		return -1;
	if (!rte_log_can_log(logtype, level))
		return 0;

	/* save loglevel and logtype in a global per-lcore variable */
	RTE_PER_LCORE(log_cur_msg).loglevel = level;
	RTE_PER_LCORE(log_cur_msg).logtype = logtype;

	ret = (*rte_logs.print_func)(f, format, ap);
	fflush(f);
	return ret;
}

/*
 * Generates a log message The message will be sent in the stream
 * defined by the previous call to rte_openlog_stream().
 * No need to check level here, done by rte_vlog().
 */
RTE_EXPORT_SYMBOL(rte_log)
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
 * Called by rte_eal_init
 */
RTE_EXPORT_INTERNAL_SYMBOL(eal_log_init)
void
eal_log_init(const char *id)
{
	/* If user has already set a log stream, then use it. */
	if (rte_logs.file == NULL) {
		FILE *logf = NULL;

		/* if stderr is associated with systemd environment */
		if (log_journal_enabled())
			logf = log_journal_open(id);
		/* If --syslog option was passed */
		else if (log_syslog_enabled())
			logf = log_syslog_open(id);

		/* if either syslog or journal is used, then no special handling */
		if (logf) {
			rte_openlog_stream(logf);
			rte_logs.is_internal_file = true;
		} else {
			bool is_terminal = isatty(fileno(stderr));
			bool use_color = log_color_enabled(is_terminal);

			if (log_timestamp_enabled()) {
				if (use_color)
					rte_logs.print_func = color_print_with_timestamp;
				else
					rte_logs.print_func = log_print_with_timestamp;
			} else {
				if (use_color)
					rte_logs.print_func = color_print;
				else
					rte_logs.print_func = vfprintf;
			}
		}
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	RTE_LOG(NOTICE, EAL,
		"Debug dataplane logs available - lower performance\n");
#endif
}

/*
 * Called by eal_cleanup
 */
RTE_EXPORT_INTERNAL_SYMBOL(rte_eal_log_cleanup)
void
rte_eal_log_cleanup(void)
{
	if (rte_logs.is_internal_file && rte_logs.file != NULL)
		fclose(rte_logs.file);
	rte_logs.file = NULL;
	rte_logs.is_internal_file = false;
}
