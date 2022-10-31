/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _IDPF_LOGS_H_
#define _IDPF_LOGS_H_

#include <rte_log.h>

extern int idpf_logtype_init;
extern int idpf_logtype_driver;

#define PMD_INIT_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		idpf_logtype_init, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))

#define PMD_DRV_LOG_RAW(level, ...) \
	rte_log(RTE_LOG_ ## level, \
		idpf_logtype_driver, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#ifdef RTE_LIBRTE_IDPF_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG(level, \
		PMD, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_IDPF_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG(level, \
		PMD, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#endif /* _IDPF_LOGS_H_ */
