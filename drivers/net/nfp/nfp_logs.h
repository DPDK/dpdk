/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_LOGS_H__
#define __NFP_LOGS_H__

#include <rte_log.h>

extern int nfp_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_init, \
		"%s(): " fmt "\n", __func__, ## args)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int nfp_logtype_rx;
#define PMD_RX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_rx, \
		"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int nfp_logtype_tx;
#define PMD_TX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_tx, \
		"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

extern int nfp_logtype_cpp;
#define PMD_CPP_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_cpp, \
		"%s(): " fmt "\n", __func__, ## args)

extern int nfp_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_driver, \
		"%s(): " fmt "\n", __func__, ## args)

#endif /* __NFP_LOGS_H__ */
