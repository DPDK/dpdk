/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rte_log.h>
#include <rte_string_fns.h>

#include "power_common.h"

#define POWER_SYSFILE_SCALING_DRIVER   \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_driver"
#define POWER_SYSFILE_GOVERNOR  \
		"/sys/devices/system/cpu/cpu%u/cpufreq/scaling_governor"
#define POWER_CONVERT_TO_DECIMAL 10

int
cpufreq_check_scaling_driver(const char *driver_name)
{
	unsigned int lcore_id = 0; /* always check core 0 */
	char readbuf[PATH_MAX];
	size_t end_idx;
	char *s;
	FILE *f;

	/*
	 * Check if scaling driver matches what we expect.
	 */
	open_core_sysfs_file(&f, "r", POWER_SYSFILE_SCALING_DRIVER,
			lcore_id);
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

int
open_core_sysfs_file(FILE **f, const char *mode, const char *format, ...)
{
	char fullpath[PATH_MAX];
	va_list ap;
	FILE *tmpf;

	va_start(ap, format);
	vsnprintf(fullpath, sizeof(fullpath), format, ap);
	va_end(ap);
	tmpf = fopen(fullpath, mode);
	*f = tmpf;
	if (tmpf == NULL)
		return -1;

	return 0;
}

int
read_core_sysfs_u32(FILE *f, uint32_t *val)
{
	char buf[BUFSIZ];
	uint32_t fval;
	char *s;

	s = fgets(buf, sizeof(buf), f);
	if (s == NULL)
		return -1;

	/* fgets puts null terminator in, but do this just in case */
	buf[BUFSIZ - 1] = '\0';

	/* strip off any terminating newlines */
	*strchrnul(buf, '\n') = '\0';

	fval = strtoul(buf, NULL, POWER_CONVERT_TO_DECIMAL);

	/* write the value */
	*val = fval;

	return 0;
}

int
read_core_sysfs_s(FILE *f, char *buf, unsigned int len)
{
	char *s;

	s = fgets(buf, len, f);
	if (s == NULL)
		return -1;

	/* fgets puts null terminator in, but do this just in case */
	buf[len - 1] = '\0';

	/* strip off any terminating newlines */
	*strchrnul(buf, '\n') = '\0';

	return 0;
}

int
write_core_sysfs_s(FILE *f, const char *str)
{
	int ret;

	ret = fseek(f, 0, SEEK_SET);
	if (ret != 0)
		return -1;

	ret = fputs(str, f);
	if (ret < 0)
		return -1;

	/* flush the output */
	ret = fflush(f);
	if (ret != 0)
		return -1;

	return 0;
}

/**
 * It is to check the current scaling governor by reading sys file, and then
 * set it into 'performance' if it is not by writing the sys file. The original
 * governor will be saved for rolling back.
 */
int
power_set_governor(unsigned int lcore_id, const char *new_governor,
		char *orig_governor, size_t orig_governor_len)
{
	FILE *f_governor = NULL;
	int ret = -1;
	char buf[BUFSIZ];

	open_core_sysfs_file(&f_governor, "rw+", POWER_SYSFILE_GOVERNOR,
			lcore_id);
	if (f_governor == NULL) {
		RTE_LOG(ERR, POWER, "failed to open %s\n",
				POWER_SYSFILE_GOVERNOR);
		goto out;
	}

	ret = read_core_sysfs_s(f_governor, buf, sizeof(buf));
	if (ret < 0) {
		RTE_LOG(ERR, POWER, "Failed to read %s\n",
				POWER_SYSFILE_GOVERNOR);
		goto out;
	}

	/* Save the original governor, if it was provided */
	if (orig_governor)
		rte_strscpy(orig_governor, buf, orig_governor_len);

	/* Check if current governor is already what we want */
	if (strcmp(buf, new_governor) == 0) {
		ret = 0;
		POWER_DEBUG_TRACE("Power management governor of lcore %u is "
				"already %s\n", lcore_id, new_governor);
		goto out;
	}

	/* Write the new governor */
	ret = write_core_sysfs_s(f_governor, new_governor);
	if (ret < 0) {
		RTE_LOG(ERR, POWER, "Failed to write %s\n",
				POWER_SYSFILE_GOVERNOR);
		goto out;
	}

	ret = 0;
	RTE_LOG(INFO, POWER, "Power management governor of lcore %u has been "
			"set to '%s' successfully\n", lcore_id, new_governor);
out:
	if (f_governor != NULL)
		fclose(f_governor);

	return ret;
}
