/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_VDPA_LOG_H__
#define __NFP_VDPA_LOG_H__

#include <rte_log.h>

extern int nfp_logtype_vdpa;
#define DRV_VDPA_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_vdpa, \
		"NFP_VDPA: %s(): " fmt "\n", __func__, ## args)

extern int nfp_logtype_core;
#define DRV_CORE_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_core, \
		"NFP_CORE: %s(): " fmt "\n", __func__, ## args)

#endif /* __NFP_VDPA_LOG_H__ */
