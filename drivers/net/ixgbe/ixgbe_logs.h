/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _IXGBE_LOGS_H_
#define _IXGBE_LOGS_H_

#define PMD_INIT_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ##args)

#ifdef RTE_LIBRTE_IXGBE_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while(0)
#endif

#ifdef RTE_LIBRTE_IXGBE_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_IXGBE_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_IXGBE_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_IXGBE_DEBUG_DRIVER
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt, __func__, ## args)
#else
#define PMD_DRV_LOG_RAW(level, fmt, args...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#endif /* _IXGBE_LOGS_H_ */
