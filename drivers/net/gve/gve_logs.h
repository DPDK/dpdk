/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#ifndef _GVE_LOGS_H_
#define _GVE_LOGS_H_

extern int gve_logtype_driver;
#define RTE_LOGTYPE_GVE_DRIVER gve_logtype_driver

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, GVE_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#define PMD_DRV_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE_PREFIX(level, GVE_DRIVER, "%s(): ", __func__,  __VA_ARGS__)

#endif
