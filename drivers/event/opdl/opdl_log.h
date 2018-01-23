/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _OPDL_LOGS_H_
#define _OPDL_LOGS_H_

#include <rte_log.h>

extern int opdl_logtype_driver;

#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, opdl_logtype_driver, "%s(): " fmt, \
			__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)



#endif /* _OPDL_LOGS_H_ */
