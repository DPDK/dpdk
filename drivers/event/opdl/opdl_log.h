/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _OPDL_LOGS_H_
#define _OPDL_LOGS_H_

#include <rte_log.h>

extern int opdl_logtype_driver;
#define RTE_LOGTYPE_OPDL opdl_logtype_driver

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, OPDL, "%s(): ", __func__, __VA_ARGS__)

#endif /* _OPDL_LOGS_H_ */
