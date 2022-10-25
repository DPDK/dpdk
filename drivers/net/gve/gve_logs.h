/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#ifndef _GVE_LOGS_H_
#define _GVE_LOGS_H_

extern int gve_logtype_driver;

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, gve_logtype_driver, "%s(): " fmt "\n", \
		__func__, ## args)

#endif
