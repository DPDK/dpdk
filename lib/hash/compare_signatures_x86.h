/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 * Copyright(c) 2018-2024 Arm Limited
 */

#ifndef COMPARE_SIGNATURES_X86_H
#define COMPARE_SIGNATURES_X86_H

#include <inttypes.h>

#include <rte_common.h>
#include <rte_vect.h>

#include "rte_cuckoo_hash.h"

/* x86's version uses a sparsely packed hitmask buffer: every other bit is padding. */
#define DENSE_HASH_BULK_LOOKUP 0

static inline void
compare_signatures_sparse(uint32_t *prim_hash_matches, uint32_t *sec_hash_matches,
			const struct rte_hash_bucket *prim_bkt,
			const struct rte_hash_bucket *sec_bkt,
			uint16_t sig,
			enum rte_hash_sig_compare_function sig_cmp_fn)
{
	unsigned int i;

	/* For match mask the first bit of every two bits indicates the match */
	switch (sig_cmp_fn) {
#if defined(__SSE2__) && RTE_HASH_BUCKET_ENTRIES <= 8
	case RTE_HASH_COMPARE_SSE:
		/* Compare all signatures in the bucket */
		*prim_hash_matches = _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_load_si128(
			(__m128i const *)prim_bkt->sig_current), _mm_set1_epi16(sig)));
		/* Extract the even-index bits only */
		*prim_hash_matches &= 0x5555;
		/* Compare all signatures in the bucket */
		*sec_hash_matches = _mm_movemask_epi8(_mm_cmpeq_epi16(_mm_load_si128(
			(__m128i const *)sec_bkt->sig_current), _mm_set1_epi16(sig)));
		/* Extract the even-index bits only */
		*sec_hash_matches &= 0x5555;
		break;
#endif
	default:
		for (i = 0; i < RTE_HASH_BUCKET_ENTRIES; i++) {
			*prim_hash_matches |= (sig == prim_bkt->sig_current[i]) << (i << 1);
			*sec_hash_matches |= (sig == sec_bkt->sig_current[i]) << (i << 1);
		}
	}
}
#endif /* COMPARE_SIGNATURES_X86_H */
