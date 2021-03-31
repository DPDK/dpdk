/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _IXGBE_LOGS_H_
#define _IXGBE_LOGS_H_

extern int ixgbe_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_init, \
		"%s(): " fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_ETHDEV_DEBUG_RX
extern int ixgbe_logtype_rx;
#define PMD_RX_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_rx,	\
		"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int ixgbe_logtype_tx;
#define PMD_TX_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_tx,	\
		"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

extern int ixgbe_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ixgbe_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#endif /* _IXGBE_LOGS_H_ */
