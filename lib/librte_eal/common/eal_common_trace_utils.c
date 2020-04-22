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
#include "eal_trace.h"

static bool
trace_entry_compare(const char *name)
{
	struct trace_point_head *tp_list = trace_list_head_get();
	struct trace_point *tp;
	int count = 0;

	STAILQ_FOREACH(tp, tp_list, next) {
		if (strncmp(tp->name, name, TRACE_POINT_NAME_SIZE) == 0)
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
trace_session_name_generate(char *trace_dir)
{
	struct tm *tm_result;
	time_t tm;
	int rc;

	tm = time(NULL);
	if ((int)tm == -1)
		goto fail;

	tm_result = localtime(&tm);
	if (tm_result == NULL)
		goto fail;

	rc = rte_strscpy(trace_dir, eal_get_hugefile_prefix(),
			TRACE_PREFIX_LEN);
	if (rc == -E2BIG)
		rc = TRACE_PREFIX_LEN;
	trace_dir[rc++] = '-';

	rc = strftime(trace_dir + rc, TRACE_DIR_STR_LEN - rc,
			"%Y-%m-%d-%p-%I-%M-%S", tm_result);
	if (rc == 0)
		goto fail;

	return rc;
fail:
	rte_errno = errno;
	return -rte_errno;
}

static int
trace_dir_default_path_get(char *dir_path)
{
	struct trace *trace = trace_obj_get();
	uint32_t size = sizeof(trace->dir);
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
	if (snprintf(dir_path, size, "%s/dpdk-traces/", home_dir) < 0)
		return -ENAMETOOLONG;

	return 0;
}

int
trace_mkdir(void)
{
	struct trace *trace = trace_obj_get();
	char session[TRACE_DIR_STR_LEN];
	char *dir_path;
	int rc;

	if (!trace->dir_offset) {
		dir_path = calloc(1, sizeof(trace->dir));
		if (dir_path == NULL) {
			trace_err("fail to allocate memory");
			return -ENOMEM;
		}

		rc = trace_dir_default_path_get(dir_path);
		if (rc < 0) {
			trace_err("fail to get default path");
			free(dir_path);
			return rc;
		}

	}

	/* Create the path if it t exist, no "mkdir -p" available here */
	rc = mkdir(trace->dir, 0700);
	if (rc < 0 && errno != EEXIST) {
		trace_err("mkdir %s failed [%s]", trace->dir, strerror(errno));
		rte_errno = errno;
		return -rte_errno;
	}

	rc = trace_session_name_generate(session);
	if (rc < 0)
		return rc;

	rc = mkdir(trace->dir, 0700);
	if (rc < 0) {
		trace_err("mkdir %s failed [%s]", trace->dir, strerror(errno));
		rte_errno = errno;
		return -rte_errno;
	}

	RTE_LOG(INFO, EAL, "Trace dir: %s\n", trace->dir);
	return 0;
}

