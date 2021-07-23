/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef _ENA_LOGS_H_
#define _ENA_LOGS_H_

extern int ena_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ena_logtype_init, \
		"%s(): " fmt, __func__, ## args)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int ena_logtype_rx;
#define PMD_RX_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, ena_logtype_rx,	\
		"%s(): " fmt, __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int ena_logtype_tx;
#define PMD_TX_LOG(level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, ena_logtype_tx,	\
		"%s(): " fmt, __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

extern int ena_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, ena_logtype_driver, \
		"%s(): " fmt, __func__, ## args)

#endif /* _ENA_LOGS_H_ */
