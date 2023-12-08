/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation.
 * Copyright(c) 2012-2014 6WIND S.A.
 */

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rte_log.h>

#include "eal_private.h"
#include "eal_filesystem.h"

int eal_create_runtime_dir(void)
{
	const char *directory;
	char run_dir[PATH_MAX];
	char tmp[PATH_MAX];
	int ret;

	/* from RuntimeDirectory= see systemd.exec */
	directory = getenv("RUNTIME_DIRECTORY");
	if (directory == NULL) {
		/*
		 * Used standard convention defined in
		 * XDG Base Directory Specification and
		 * Filesystem Hierarchy Standard.
		 */
		if (getuid() == 0)
			directory = "/var/run";
		else
			directory = getenv("XDG_RUNTIME_DIR") ? : "/tmp";
	}

	/* create DPDK subdirectory under runtime dir */
	ret = snprintf(tmp, sizeof(tmp), "%s/dpdk", directory);
	if (ret < 0 || ret == sizeof(tmp)) {
		EAL_LOG(ERR, "Error creating DPDK runtime path name");
		return -1;
	}

	/* create prefix-specific subdirectory under DPDK runtime dir */
	ret = snprintf(run_dir, sizeof(run_dir), "%s/%s",
			tmp, eal_get_hugefile_prefix());
	if (ret < 0 || ret == sizeof(run_dir)) {
		EAL_LOG(ERR, "Error creating prefix-specific runtime path name");
		return -1;
	}

	/* create the path if it doesn't exist. no "mkdir -p" here, so do it
	 * step by step.
	 */
	ret = mkdir(tmp, 0700);
	if (ret < 0 && errno != EEXIST) {
		EAL_LOG(ERR, "Error creating '%s': %s",
			tmp, strerror(errno));
		return -1;
	}

	ret = mkdir(run_dir, 0700);
	if (ret < 0 && errno != EEXIST) {
		EAL_LOG(ERR, "Error creating '%s': %s",
			run_dir, strerror(errno));
		return -1;
	}

	if (eal_set_runtime_dir(run_dir))
		return -1;

	return 0;
}

/* parse a sysfs (or other) file containing one integer value */
int eal_parse_sysfs_value(const char *filename, unsigned long *val)
{
	FILE *f;
	char buf[BUFSIZ];
	char *end = NULL;

	if ((f = fopen(filename, "r")) == NULL) {
		EAL_LOG(ERR, "%s(): cannot open sysfs value %s",
			__func__, filename);
		return -1;
	}

	if (fgets(buf, sizeof(buf), f) == NULL) {
		EAL_LOG(ERR, "%s(): cannot read sysfs value %s",
			__func__, filename);
		fclose(f);
		return -1;
	}
	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
		EAL_LOG(ERR, "%s(): cannot parse sysfs value %s",
				__func__, filename);
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}
