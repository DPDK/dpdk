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

static inline void
compare_signatures(uint32_t *prim_hash_matches, uint32_t *sec_hash_matches,
			const struct rte_hash_bucket *prim_bkt,
			const struct rte_hash_bucket *sec_bkt,
			uint16_t sig,
			enum rte_hash_sig_compare_function sig_cmp_fn)
{
	unsigned int i;

	/* For match mask the first bit of every two bits indicates the match */
	switch (sig_cmp_fn) {
#if defined(__ARM_NEON) && RTE_HASH_BUCKET_ENTRIES <= 8
	case RTE_HASH_COMPARE_NEON: {
		uint16x8_t vmat, vsig, x;
		int16x8_t shift = {-15, -13, -11, -9, -7, -5, -3, -1};

		vsig = vld1q_dup_u16((uint16_t const *)&sig);
		/* Compare all signatures in the primary bucket */
		vmat = vceqq_u16(vsig,
			vld1q_u16((uint16_t const *)prim_bkt->sig_current));
		x = vshlq_u16(vandq_u16(vmat, vdupq_n_u16(0x8000)), shift);
		*prim_hash_matches = (uint32_t)(vaddvq_u16(x));
		/* Compare all signatures in the secondary bucket */
		vmat = vceqq_u16(vsig,
			vld1q_u16((uint16_t const *)sec_bkt->sig_current));
		x = vshlq_u16(vandq_u16(vmat, vdupq_n_u16(0x8000)), shift);
		*sec_hash_matches = (uint32_t)(vaddvq_u16(x));
		break;
	}
#endif
	default:
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			*prim_hash_matches |=
				((sig == prim_bkt->sig_current[i]) << (i << 1));
			*sec_hash_matches |=
				((sig == sec_bkt->sig_current[i]) << (i << 1));
		}
	}
}
#endif /* COMPARE_SIGNATURES_ARM_H */
