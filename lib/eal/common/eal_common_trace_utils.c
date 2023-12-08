/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <fnmatch.h>
#include <pwd.h>
#include <sys/stat.h>
#include <time.h>

#include <rte_common.h>
#include <rte_errno.h>
#include <rte_string_fns.h>

#include "eal_filesystem.h"
#include "eal_private.h"
#include "eal_trace.h"

const char *
trace_mode_to_string(enum rte_trace_mode mode)
{
	switch (mode) {
	case RTE_TRACE_MODE_OVERWRITE: return "overwrite";
	case RTE_TRACE_MODE_DISCARD: return "discard";
	default: return "unknown";
	}
}

const char *
trace_area_to_string(enum trace_area_e area)
{
	switch (area) {
	case TRACE_AREA_HEAP: return "heap";
	case TRACE_AREA_HUGEPAGE: return "hugepage";
	default: return "unknown";
	}
}

static bool
trace_entry_compare(const char *name)
{
	struct trace_point_head *tp_list = trace_list_head_get();
	struct trace_point *tp;
	int count = 0;

	STAILQ_FOREACH(tp, tp_list, next) {
		if (strcmp(tp->name, name) == 0)
			count++;
		if (count > 1) {
			trace_err("found duplicate entry %s", name);
			rte_errno = EEXIST;
			return true;
		}
	}
	return false;
}

bool
trace_has_duplicate_entry(void)
{
	struct trace_point_head *tp_list = trace_list_head_get();
	struct trace_point *tp;

	/* Is duplicate trace name registered */
	STAILQ_FOREACH(tp, tp_list, next)
		if (trace_entry_compare(tp->name))
			return true;

	return false;
}

void
trace_uuid_generate(void)
{
	struct trace_point_head *tp_list = trace_list_head_get();
	struct trace *trace = trace_obj_get();
	struct trace_point *tp;
	uint64_t sz_total = 0;

	/* Go over the registered trace points to get total size of events */
	STAILQ_FOREACH(tp, tp_list, next) {
		const uint16_t sz = *tp->handle & __RTE_TRACE_FIELD_SIZE_MASK;
		sz_total += sz;
	}

	rte_uuid_t uuid = RTE_UUID_INIT(sz_total, trace->nb_trace_points,
		0x4370, 0x8f50, 0x222ddd514176ULL);
	rte_uuid_copy(trace->uuid, uuid);
}

static int
trace_session_name_generate(char **trace_dir)
{
	char date[sizeof("YYYY-mm-dd-AM-HH-MM-SS")];
	struct tm *tm_result;
	time_t tm;

	tm = time(NULL);
	if ((int)tm == -1)
		goto fail;

	tm_result = localtime(&tm);
	if (tm_result == NULL)
		goto fail;

	if (strftime(date, sizeof(date), "%Y-%m-%d-%p-%I-%M-%S", tm_result) == 0) {
		errno = ENOSPC;
		goto fail;
	}

	if (asprintf(trace_dir, "%s-%s", eal_get_hugefile_prefix(), date) == -1)
		goto fail;

	return 0;
fail:
	rte_errno = errno;
	return -1;
}

static int
trace_dir_update(const char *str)
{
	struct trace *trace = trace_obj_get();
	char *dir;
	int rc;

	rc = asprintf(&dir, "%s%s", trace->dir != NULL ? trace->dir : "", str);
	if (rc != -1) {
		free(trace->dir);
		trace->dir = dir;
	}
	return rc;
}

int
eal_trace_args_save(const char *val)
{
	struct trace *trace = trace_obj_get();
	struct trace_arg *arg = malloc(sizeof(*arg));

	if (arg == NULL) {
		trace_err("failed to allocate memory for %s", val);
		return -ENOMEM;
	}

	arg->val = strdup(val);
	if (arg->val == NULL) {
		trace_err("failed to allocate memory for %s", val);
		free(arg);
		return -ENOMEM;
	}

	STAILQ_INSERT_TAIL(&trace->args, arg, next);
	return 0;
}

void
eal_trace_args_free(void)
{
	struct trace *trace = trace_obj_get();
	struct trace_arg *arg;

	while (!STAILQ_EMPTY(&trace->args)) {
		arg = STAILQ_FIRST(&trace->args);
		STAILQ_REMOVE_HEAD(&trace->args, next);
		free(arg->val);
		free(arg);
	}
}

int
trace_args_apply(const char *arg)
{
	if (rte_trace_regexp(arg, true) < 0) {
		trace_err("cannot enable trace for %s", arg);
		return -1;
	}

	return 0;
}

int
eal_trace_bufsz_args_save(char const *val)
{
	struct trace *trace = trace_obj_get();
	uint64_t bufsz;

	bufsz = rte_str_to_size(val);
	if (bufsz == 0) {
		trace_err("buffer size cannot be zero");
		return -EINVAL;
	}

	trace->buff_len = bufsz;
	return 0;
}

void
trace_bufsz_args_apply(void)
{
	struct trace *trace = trace_obj_get();

	if (trace->buff_len == 0)
		trace->buff_len = 1024 * 1024; /* 1MB */
}

int
eal_trace_mode_args_save(const char *val)
{
	struct trace *trace = trace_obj_get();
	size_t len = strlen(val);
	unsigned long tmp;
	char *pattern;

	if (len == 0) {
		trace_err("value is not provided with option");
		return -EINVAL;
	}

	pattern = (char *)calloc(1, len + 2);
	if (pattern == NULL) {
		trace_err("fail to allocate memory");
		return -ENOMEM;
	}

	sprintf(pattern, "%s*", val);

	if (fnmatch(pattern, "overwrite", 0) == 0)
		tmp = RTE_TRACE_MODE_OVERWRITE;
	else if (fnmatch(pattern, "discard", 0) == 0)
		tmp = RTE_TRACE_MODE_DISCARD;
	else {
		free(pattern);
		return -EINVAL;
	}

	trace->mode = tmp;
	free(pattern);
	return 0;
}

int
eal_trace_dir_args_save(char const *val)
{
	char *dir_path;
	int rc;

	if (asprintf(&dir_path, "%s/", val) == -1) {
		trace_err("failed to copy directory: %s", strerror(errno));
		return -ENOMEM;
	}

	rc = trace_dir_update(dir_path);
	free(dir_path);
	return rc;
}

int
trace_epoch_time_save(void)
{
	struct trace *trace = trace_obj_get();
	struct timespec epoch = { 0, 0 };
	uint64_t avg, start, end;

	start = rte_get_tsc_cycles();
	if (clock_gettime(CLOCK_REALTIME, &epoch) < 0) {
		trace_err("failed to get the epoch time");
		return -1;
	}
	end = rte_get_tsc_cycles();
	avg = (start + end) >> 1;

	trace->epoch_sec = (uint64_t) epoch.tv_sec;
	trace->epoch_nsec = (uint64_t) epoch.tv_nsec;
	trace->uptime_ticks = avg;

	return 0;
}

static int
trace_dir_default_path_get(char **dir_path)
{
	struct passwd *pwd;
	char *home_dir;

	/* First check for shell environment variable */
	home_dir = getenv("HOME");
	if (home_dir == NULL) {
		/* Fallback to password file entry */
		pwd = getpwuid(getuid());
		if (pwd == NULL)
			return -EINVAL;

		home_dir = pwd->pw_dir;
	}

	/* Append dpdk-traces to directory */
	if (asprintf(dir_path, "%s/dpdk-traces/", home_dir) == -1)
		return -ENOMEM;

	return 0;
}

static int
trace_mkdir(void)
{
	struct trace *trace = trace_obj_get();
	static bool already_done;
	char *session;
	int rc;

	if (already_done)
		return 0;

	if (trace->dir == NULL) {
		char *dir_path;

		rc = trace_dir_default_path_get(&dir_path);
		if (rc < 0) {
			trace_err("fail to get default path");
			return rc;
		}

		rc = trace_dir_update(dir_path);
		free(dir_path);
		if (rc < 0)
			return rc;
	}

	/* Create the path if it t exist, no "mkdir -p" available here */
	rc = mkdir(trace->dir, 0700);
	if (rc < 0 && errno != EEXIST) {
		trace_err("mkdir %s failed [%s]", trace->dir, strerror(errno));
		rte_errno = errno;
		return -rte_errno;
	}

	rc = trace_session_name_generate(&session);
	if (rc < 0)
		return rc;
	rc = trace_dir_update(session);
	free(session);
	if (rc < 0)
		return rc;

	rc = mkdir(trace->dir, 0700);
	if (rc < 0) {
		trace_err("mkdir %s failed [%s]", trace->dir, strerror(errno));
		rte_errno = errno;
		return -rte_errno;
	}

	EAL_LOG(INFO, "Trace dir: %s", trace->dir);
	already_done = true;
	return 0;
}

static int
trace_meta_save(struct trace *trace)
{
	char file_name[PATH_MAX];
	FILE *f;
	int rc;

	rc = snprintf(file_name, PATH_MAX, "%s/metadata", trace->dir);
	if (rc < 0)
		return rc;

	f = fopen(file_name, "w");
	if (f == NULL)
		return -errno;

	rc = rte_trace_metadata_dump(f);

	if (fclose(f))
		rc = -errno;

	return rc;
}


static inline int
trace_file_sz(struct __rte_trace_header *hdr)
{
	return sizeof(struct __rte_trace_stream_header) + hdr->offset;
}

static int
trace_mem_save(struct trace *trace, struct __rte_trace_header *hdr,
		uint32_t cnt)
{
	char file_name[PATH_MAX];
	FILE *f;
	int rc;

	rc = snprintf(file_name, PATH_MAX, "%s/channel0_%d", trace->dir, cnt);
	if (rc < 0)
		return rc;

	f = fopen(file_name, "w");
	if (f == NULL)
		return -errno;

	rc = fwrite(&hdr->stream_header, trace_file_sz(hdr), 1, f);
	rc = (rc == 1) ?  0 : -EACCES;

	if (fclose(f))
		rc = -errno;

	return rc;
}

int
rte_trace_save(void)
{
	struct trace *trace = trace_obj_get();
	struct __rte_trace_header *header;
	uint32_t count;
	int rc = 0;

	if (trace->nb_trace_mem_list == 0)
		return rc;

	rc = trace_mkdir();
	if (rc < 0)
		return rc;

	rc = trace_meta_save(trace);
	if (rc)
		return rc;

	rte_spinlock_lock(&trace->lock);
	for (count = 0; count < trace->nb_trace_mem_list; count++) {
		header = trace->lcore_meta[count].mem;
		rc =  trace_mem_save(trace, header, count);
		if (rc)
			break;
	}
	rte_spinlock_unlock(&trace->lock);
	return rc;
}
