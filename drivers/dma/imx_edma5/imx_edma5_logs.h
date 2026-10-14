/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2026 NXP
 */

#ifndef IMX_EDMA5_LOGS_H
#define IMX_EDMA5_LOGS_H

#include <rte_log.h>

extern int imx_edma5_logtype;
#define RTE_LOGTYPE_IMX_EDMA5 imx_edma5_logtype

#define IMX_EDMA5_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IMX_EDMA5, "%s(): ", __func__, __VA_ARGS__)

#endif /* IMX_EDMA5_LOGS_H */
