/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_DEBUG_H_
#define _ARK_DEBUG_H_

#include <inttypes.h>
#include <rte_log.h>


/* Configuration option to pad TX packets to 60 bytes */
#ifdef RTE_LIBRTE_ARK_PAD_TX
#define ARK_TX_PAD_TO_60   1
#else
#define ARK_TX_PAD_TO_60   0
#endif

/* system camel case definition changed to upper case */
#define PRIU32 PRIu32
#define PRIU64 PRIu64

/* Format specifiers for string data pairs */
#define ARK_SU32  "\n\t%-20s    %'20" PRIU32
#define ARK_SU64  "\n\t%-20s    %'20" PRIU64
#define ARK_SU64X "\n\t%-20s    %#20" PRIx64
#define ARK_SPTR  "\n\t%-20s    %20p"

extern int ark_logtype;

#define PMD_DRV_LOG(level, fmt, args...)	\
	rte_log(RTE_LOG_ ##level, ark_logtype, fmt, ## args)

/* Conditional trace definitions */
#define ARK_TRACE_ON(level, fmt, args...) \
	PMD_DRV_LOG(level, fmt, ## args)

/* This pattern allows compiler check arguments even if disabled  */
#define ARK_TRACE_OFF(level, fmt, args...)			\
	do {							\
		if (0)						\
			PMD_DRV_LOG(level, fmt, ## args);	\
	} while (0)

/* tracing including the function name */
#define ARK_FUNC_ON(level, fmt, args...) \
	PMD_DRV_LOG(level, "%s(): " fmt, __func__, ## args)

/* tracing including the function name */
#define ARK_FUNC_OFF(level, fmt, args...)				\
	do {								\
		if (0)							\
			PMD_DRV_LOG(level, "%s(): " fmt, __func__, ## args); \
	} while (0)


/* Debug macro for tracing full behavior, function tracing and messages*/
#ifdef RTE_LIBRTE_ARK_DEBUG_TRACE
#define PMD_FUNC_LOG(level, fmt, ...) ARK_FUNC_ON(level, fmt, ##__VA_ARGS__)
#define PMD_DEBUG_LOG(level, fmt, ...) ARK_TRACE_ON(level, fmt, ##__VA_ARGS__)
#else
#define PMD_FUNC_LOG(level, fmt, ...) ARK_FUNC_OFF(level, fmt, ##__VA_ARGS__)
#define PMD_DEBUG_LOG(level, fmt, ...) ARK_TRACE_OFF(level, fmt, ##__VA_ARGS__)
#endif


/* Debug macro for reporting FPGA statistics */
#ifdef RTE_LIBRTE_ARK_DEBUG_STATS
#define PMD_STATS_LOG(level, fmt, ...) ARK_TRACE_ON(level, fmt, ##__VA_ARGS__)
#else
#define PMD_STATS_LOG(level, fmt, ...)  ARK_TRACE_OFF(level, fmt, ##__VA_ARGS__)
#endif


/* Debug macro for RX path */
#ifdef RTE_LIBRTE_ARK_DEBUG_RX
#define ARK_RX_DEBUG 1
#define PMD_RX_LOG(level, fmt, ...)  ARK_TRACE_ON(level, fmt, ##__VA_ARGS__)
#else
#define ARK_RX_DEBUG 0
#define PMD_RX_LOG(level, fmt, ...)  ARK_TRACE_OFF(level, fmt, ##__VA_ARGS__)
#endif

/* Debug macro for TX path */
#ifdef RTE_LIBRTE_ARK_DEBUG_TX
#define ARK_TX_DEBUG       1
#define PMD_TX_LOG(level, fmt, ...)  ARK_TRACE_ON(level, fmt, ##__VA_ARGS__)
#else
#define ARK_TX_DEBUG       0
#define PMD_TX_LOG(level, fmt, ...)  ARK_TRACE_OFF(level, fmt, ##__VA_ARGS__)
#endif

#endif
