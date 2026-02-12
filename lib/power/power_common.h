/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef POWER_COMMON_H
#define POWER_COMMON_H

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_log.h>

#define RTE_POWER_INVALID_FREQ_INDEX (~0)

extern int rte_power_logtype;
#define RTE_LOGTYPE_POWER rte_power_logtype
#define POWER_LOG(level, ...) \
	RTE_LOG_LINE(level, POWER, "" __VA_ARGS__)

#ifdef RTE_LIBRTE_POWER_DEBUG
#define POWER_DEBUG_LOG(...) \
	RTE_LOG_LINE_PREFIX(ERR, POWER, "%s: ", __func__, __VA_ARGS__)
#else
#define POWER_DEBUG_LOG(...)
#endif

/* check if scaling driver matches one we want */
__rte_internal
int cpufreq_check_scaling_driver(const char *driver);

__rte_internal
int power_set_governor(unsigned int lcore_id, const char *new_governor,
		char *orig_governor, size_t orig_governor_len);

__rte_internal
int open_core_sysfs_file(FILE **f, const char *mode, const char *format, ...)
		__rte_format_printf(3, 4);

__rte_internal
int read_core_sysfs_u32(FILE *f, uint32_t *val);

__rte_internal
int read_core_sysfs_s(FILE *f, char *buf, unsigned int len);

__rte_internal
int write_core_sysfs_s(FILE *f, const char *str);

__rte_internal
int power_get_lcore_mapped_cpu_id(uint32_t lcore_id, uint32_t *cpu_id);

#endif /* POWER_COMMON_H */
