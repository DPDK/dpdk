/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright(c) 2018-2024 Arm Limited
 */

#ifndef COMPARE_SIGNATURES_ARM_H
#define COMPARE_SIGNATURES_ARM_H

#include <inttypes.h>

#include <rte_common.h>
#include <rte_vect.h>

#include "rte_cuckoo_hash.h"

/* Arm's version uses a densely packed hitmask buffer: every bit is in use. */
#define DENSE_HASH_BULK_LOOKUP 1

static inline void
compare_signatures_dense(uint16_t *hitmask_buffer,
			const uint16_t *prim_bucket_sigs,
			const uint16_t *sec_bucket_sigs,
			uint16_t sig,
			enum rte_hash_sig_compare_function sig_cmp_fn)
{
	static_assert(sizeof(*hitmask_buffer) >= 2 * (RTE_HASH_BUCKET_ENTRIES / 8),
		"hitmask_buffer must be wide enough to fit a dense hitmask");

	/* For match mask every bits indicates the match */
	switch (sig_cmp_fn) {
#if defined(__ARM_NEON) && RTE_HASH_BUCKET_ENTRIES <= 8
	case RTE_HASH_COMPARE_NEON: {
		uint16x8_t vmat, vsig, x;
		int16x8_t shift = {0, 1, 2, 3, 4, 5, 6, 7};
		uint16_t low, high;

		vsig = vld1q_dup_u16((uint16_t const *)&sig);
		/* Compare all signatures in the primary bucket */
		vmat = vceqq_u16(vsig, vld1q_u16((uint16_t const *)prim_bucket_sigs));
		x = vshlq_u16(vandq_u16(vmat, vdupq_n_u16(0x0001)), shift);
		low = (uint16_t)(vaddvq_u16(x));
		/* Compare all signatures in the secondary bucket */
		vmat = vceqq_u16(vsig, vld1q_u16((uint16_t const *)sec_bucket_sigs));
		x = vshlq_u16(vandq_u16(vmat, vdupq_n_u16(0x0001)), shift);
		high = (uint16_t)(vaddvq_u16(x));
		*hitmask_buffer = low | high << RTE_HASH_BUCKET_ENTRIES;
		break;
	}
#endif
	default:
		for (unsigned int i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			*hitmask_buffer |= (sig == prim_bucket_sigs[i]) << i;
			*hitmask_buffer |=
				((sig == sec_bucket_sigs[i]) << i) << RTE_HASH_BUCKET_ENTRIES;
		}
	}
}
#endif /* COMPARE_SIGNATURES_ARM_H */
