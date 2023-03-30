/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_LOGS_H_
#define _CPFL_LOGS_H_

#include <rte_log.h>

extern int cpfl_logtype_init;
extern int cpfl_logtype_driver;

#define PMD_INIT_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		cpfl_logtype_init, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))

#define PMD_DRV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		cpfl_logtype_driver, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))

#endif /* _CPFL_LOGS_H_ */
