/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_version.h>

const char *
rte_version_prefix(void) { return RTE_VER_PREFIX; }

unsigned int
rte_version_year(void) { return RTE_VER_YEAR; }

unsigned int
rte_version_month(void) { return RTE_VER_MONTH; }

unsigned int
rte_version_minor(void) { return RTE_VER_MINOR; }

const char *
rte_version_suffix(void) { return RTE_VER_SUFFIX; }

unsigned int
rte_version_release(void) { return RTE_VER_RELEASE; }

const char *
rte_version(void)
{
	static char version[32];
	if (version[0] != 0)
		return version;
	if (strlen(RTE_VER_SUFFIX) == 0)
		snprintf(version, sizeof(version), "%s %d.%02d.%d",
				RTE_VER_PREFIX,
				RTE_VER_YEAR,
				RTE_VER_MONTH,
				RTE_VER_MINOR);
		else
			snprintf(version, sizeof(version), "%s %d.%02d.%d%s%d",
				RTE_VER_PREFIX,
				RTE_VER_YEAR,
				RTE_VER_MONTH,
				RTE_VER_MINOR,
				RTE_VER_SUFFIX,
				RTE_VER_RELEASE);
	return version;
}
