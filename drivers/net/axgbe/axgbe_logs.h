/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _AXGBE_LOGS_H_
#define _AXGBE_LOGS_H_

#include <stdio.h>

extern int axgbe_logtype_init;
#define RTE_LOGTYPE_AXGBE_INIT axgbe_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AXGBE_INIT, "%s(): ", __func__, __VA_ARGS__)

#ifdef RTE_LIBRTE_AXGBE_PMD_DEBUG
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif

extern int axgbe_logtype_driver;
#define RTE_LOGTYPE_AXGBE_DRIVER axgbe_logtype_driver
#define PMD_DRV_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AXGBE_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _AXGBE_LOGS_H_ */
