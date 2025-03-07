/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <eal_export.h>
#include <rte_version.h>

RTE_EXPORT_SYMBOL(rte_version_prefix)
const char *
rte_version_prefix(void) { return RTE_VER_PREFIX; }

RTE_EXPORT_SYMBOL(rte_version_year)
unsigned int
rte_version_year(void) { return RTE_VER_YEAR; }

RTE_EXPORT_SYMBOL(rte_version_month)
unsigned int
rte_version_month(void) { return RTE_VER_MONTH; }

RTE_EXPORT_SYMBOL(rte_version_minor)
unsigned int
rte_version_minor(void) { return RTE_VER_MINOR; }

RTE_EXPORT_SYMBOL(rte_version_suffix)
const char *
rte_version_suffix(void) { return RTE_VER_SUFFIX; }

RTE_EXPORT_SYMBOL(rte_version_release)
unsigned int
rte_version_release(void) { return RTE_VER_RELEASE; }

RTE_EXPORT_SYMBOL(rte_version)
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
