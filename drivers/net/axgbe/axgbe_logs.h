/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#ifndef _AXGBE_LOGS_H_
#define _AXGBE_LOGS_H_

#include <stdio.h>

extern int axgbe_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, axgbe_logtype_init, "%s(): " fmt "\n", \
		__func__, ##args)

#ifdef RTE_LIBRTE_AXGBE_PMD_DEBUG
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif

extern int axgbe_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, axgbe_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#endif /* _AXGBE_LOGS_H_ */
