/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_LOGS_H_
#define _IDPF_COMMON_LOGS_H_

#include <rte_log.h>

extern int idpf_common_logtype;

#define DRV_LOG_RAW(level, ...)					\
	rte_log(RTE_LOG_ ## level,				\
		idpf_common_logtype,				\
		RTE_FMT("%s(): "				\
			RTE_FMT_HEAD(__VA_ARGS__,) "\n",	\
			__func__,				\
			RTE_FMT_TAIL(__VA_ARGS__,)))

#define DRV_LOG(level, fmt, args...)		\
	DRV_LOG_RAW(level, fmt "\n", ## args)

#ifdef RTE_LIBRTE_IDPF_DEBUG_RX
#define RX_LOG(level, ...) \
	RTE_LOG(level, \
		PMD, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))
#else
#define RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_IDPF_DEBUG_TX
#define TX_LOG(level, ...) \
	RTE_LOG(level, \
		PMD, \
		RTE_FMT("%s(): " \
			RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			__func__, \
			RTE_FMT_TAIL(__VA_ARGS__,)))
#else
#define TX_LOG(level, fmt, args...) do { } while (0)
#endif

#endif /* _IDPF_COMMON_LOGS_H_ */
