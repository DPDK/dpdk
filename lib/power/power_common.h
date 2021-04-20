/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _POWER_COMMON_H_
#define _POWER_COMMON_H_

#define RTE_POWER_INVALID_FREQ_INDEX (~0)

/* check if scaling driver matches one we want */
int cpufreq_check_scaling_driver(const char *driver);

#endif /* _POWER_COMMON_H_ */
