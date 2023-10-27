/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_COMMON_LOG_H__
#define __NFP_COMMON_LOG_H__

#include <rte_log.h>

extern int nfp_logtype_common;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_common, \
			"%s(): " fmt "\n", __func__, ## args)

#endif/* __NFP_COMMON_LOG_H__ */
