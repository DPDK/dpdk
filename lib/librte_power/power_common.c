/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "power_common.h"

#define POWER_SYSFILE_SCALING_DRIVER   \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_driver"

int
cpufreq_check_scaling_driver(const char *driver_name)
{
	unsigned int lcore_id = 0; /* always check core 0 */
	char fullpath[PATH_MAX];
	char readbuf[PATH_MAX];
	size_t end_idx;
	char *s;
	FILE *f;

	/*
	 * Check if scaling driver matches what we expect.
	 */
	snprintf(fullpath, sizeof(fullpath), POWER_SYSFILE_SCALING_DRIVER,
			lcore_id);
	f = fopen(fullpath, "r");

	/* if there's no driver at all, bail out */
	if (f == NULL)
		return 0;

	s = fgets(readbuf, sizeof(readbuf), f);
	/* don't need it any more */
	fclose(f);

	/* if we can't read it, consider unsupported */
	if (s == NULL)
		return 0;

	/* when read from sysfs, driver name has an extra newline at the end */
	end_idx = strnlen(readbuf, sizeof(readbuf));
	if (end_idx > 0 && readbuf[end_idx - 1] == '\n') {
		end_idx--;
		readbuf[end_idx] = '\0';
	}

	/* does the driver name match? */
	if (strncmp(readbuf, driver_name, sizeof(readbuf)) != 0)
		return 0;

	/*
	 * We might have a situation where the driver is supported, but we don't
	 * have permissions to do frequency scaling. This error should not be
	 * handled here, so consider the system to support scaling for now.
	 */
	return 1;
}
