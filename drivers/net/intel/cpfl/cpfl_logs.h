/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_LOGS_H_
#define _CPFL_LOGS_H_

#include <rte_log.h>

extern int cpfl_logtype_init;
#define RTE_LOGTYPE_CPFL_INIT cpfl_logtype_init
extern int cpfl_logtype_driver;
#define RTE_LOGTYPE_CPFL_DRIVER cpfl_logtype_driver

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, CPFL_INIT, "%s(): ",  __func__, __VA_ARGS__)

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, CPFL_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _CPFL_LOGS_H_ */
