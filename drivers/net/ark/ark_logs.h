/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2017 Atomic Rules LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 * * Neither the name of copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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



#define PMD_DRV_LOG(level, fmt, args...)	\
	RTE_LOG(level, PMD, fmt, ## args)

/* Conditional trace definitions */
#define ARK_TRACE_ON(level, fmt, ...)		\
	RTE_LOG(level, PMD, fmt, ##__VA_ARGS__)

/* This pattern allows compiler check arguments even if disabled  */
#define ARK_TRACE_OFF(level, fmt, ...)					\
	do {if (0) RTE_LOG(level, PMD, fmt, ##__VA_ARGS__); }		\
	while (0)


/* tracing including the function name */
#define ARK_FUNC_ON(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt, __func__, ## args)

/* tracing including the function name */
#define ARK_FUNC_OFF(level, fmt, args...)				\
	do { if (0) RTE_LOG(level, PMD, "%s(): " fmt, __func__, ## args); } \
	while (0)


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
