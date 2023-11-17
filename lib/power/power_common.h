/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _POWER_COMMON_H_
#define _POWER_COMMON_H_

#include <rte_common.h>
#include <rte_log.h>

#define RTE_POWER_INVALID_FREQ_INDEX (~0)

extern int power_logtype;
#define RTE_LOGTYPE_POWER power_logtype
#define POWER_LOG(level, ...) \
	RTE_LOG_LINE(level, POWER, "" __VA_ARGS__)

#ifdef RTE_LIBRTE_POWER_DEBUG
#define POWER_DEBUG_LOG(fmt, args...) \
	RTE_LOG_LINE(ERR, POWER, "%s: " fmt, __func__, ## args)
#else
#define POWER_DEBUG_LOG(fmt, args...)
#endif

/* check if scaling driver matches one we want */
int cpufreq_check_scaling_driver(const char *driver);
int power_set_governor(unsigned int lcore_id, const char *new_governor,
		char *orig_governor, size_t orig_governor_len);
int open_core_sysfs_file(FILE **f, const char *mode, const char *format, ...)
		__rte_format_printf(3, 4);
int read_core_sysfs_u32(FILE *f, uint32_t *val);
int read_core_sysfs_s(FILE *f, char *buf, unsigned int len);
int write_core_sysfs_s(FILE *f, const char *str);

#endif /* _POWER_COMMON_H_ */
