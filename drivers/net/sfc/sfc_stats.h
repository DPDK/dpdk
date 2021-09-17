/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_STATS_H
#define _SFC_STATS_H

#include <stdint.h>

#include <rte_atomic.h>

#include "sfc_tweak.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 64-bit packets and bytes counters covered by 128-bit integer
 * in order to do atomic updates to guarantee consistency if
 * required.
 */
union sfc_pkts_bytes {
	RTE_STD_C11
	struct {
		uint64_t		pkts;
		uint64_t		bytes;
	};
	rte_int128_t			pkts_bytes;
};

/**
 * Update packets and bytes counters atomically in assumption that
 * the counter is written on one core only.
 */
static inline void
sfc_pkts_bytes_add(union sfc_pkts_bytes *st, uint64_t pkts, uint64_t bytes)
{
#if SFC_SW_STATS_ATOMIC
	union sfc_pkts_bytes result;

	/* Stats are written on single core only, so just load values */
	result.pkts = st->pkts + pkts;
	result.bytes = st->bytes + bytes;

	/*
	 * Store the result atomically to guarantee that the reader
	 * core sees both counter updates together.
	 */
	__atomic_store_n(&st->pkts_bytes.int128, result.pkts_bytes.int128,
			 __ATOMIC_RELAXED);
#else
	st->pkts += pkts;
	st->bytes += bytes;
#endif
}

/**
 * Get an atomic copy of a packets and bytes counters.
 */
static inline void
sfc_pkts_bytes_get(const union sfc_pkts_bytes *st, union sfc_pkts_bytes *result)
{
#if SFC_SW_STATS_ATOMIC
	result->pkts_bytes.int128 = __atomic_load_n(&st->pkts_bytes.int128,
						    __ATOMIC_RELAXED);
#else
	*result = *st;
#endif
}

#ifdef __cplusplus
}
#endif
#endif /* _SFC_STATS_H */
