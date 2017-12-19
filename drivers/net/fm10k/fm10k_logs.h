/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2015 Intel Corporation
 */

#ifndef _FM10K_LOGS_H_
#define _FM10K_LOGS_H_

#include <rte_log.h>

#define PMD_INIT_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ##args)

#ifdef RTE_LIBRTE_FM10K_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif

#ifdef RTE_LIBRTE_FM10K_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_FM10K_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_FM10K_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_FM10K_DEBUG_DRIVER
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt, __func__, ## args)
#else
#define PMD_DRV_LOG_RAW(level, fmt, args...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#endif /* _FM10K_LOGS_H_ */
