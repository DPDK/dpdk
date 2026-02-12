/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_VDPA_LOG_H__
#define __NFP_VDPA_LOG_H__

#include <rte_log.h>

extern int nfp_logtype_vdpa;
#define RTE_LOGTYPE_NFP_VDPA nfp_logtype_vdpa
#define DRV_VDPA_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_VDPA, "%s(): ", __func__, __VA_ARGS__)

extern int nfp_logtype_core;
#define RTE_LOGTYPE_NFP_CORE nfp_logtype_core
#define DRV_CORE_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_CORE, "%s(): ", __func__, __VA_ARGS__)

#endif /* __NFP_VDPA_LOG_H__ */
