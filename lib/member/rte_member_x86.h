/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_MEMBER_X86_H_
#define _RTE_MEMBER_X86_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <x86intrin.h>

#if defined(__AVX2__)

static inline int
update_entry_search_avx(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		member_set_t set_id)
{
	uint32_t hitmask = _mm256_movemask_epi8((__m256i)_mm256_cmpeq_epi16(
		_mm256_load_si256((__m256i const *)buckets[bucket_id].sigs),
		_mm256_set1_epi16(tmp_sig)));
	if (hitmask) {
		uint32_t hit_idx = rte_ctz32(hitmask) >> 1;
		buckets[bucket_id].sets[hit_idx] = set_id;
		return 1;
	}
	return 0;
}

static inline int
search_bucket_single_avx(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		member_set_t *set_id)
{
	uint32_t hitmask = _mm256_movemask_epi8((__m256i)_mm256_cmpeq_epi16(
		_mm256_load_si256((__m256i const *)buckets[bucket_id].sigs),
		_mm256_set1_epi16(tmp_sig)));
	while (hitmask) {
		uint32_t hit_idx = rte_ctz32(hitmask) >> 1;
		if (buckets[bucket_id].sets[hit_idx] != RTE_MEMBER_NO_MATCH) {
			*set_id = buckets[bucket_id].sets[hit_idx];
			return 1;
		}
		hitmask &= ~(3U << ((hit_idx) << 1));
	}
	return 0;
}

static inline void
search_bucket_multi_avx(uint32_t bucket_id, member_sig_t tmp_sig,
				struct member_ht_bucket *buckets,
				uint32_t *counter,
				uint32_t match_per_key,
				member_set_t *set_id)
{
	uint32_t hitmask = _mm256_movemask_epi8((__m256i)_mm256_cmpeq_epi16(
		_mm256_load_si256((__m256i const *)buckets[bucket_id].sigs),
		_mm256_set1_epi16(tmp_sig)));
	while (hitmask) {
		uint32_t hit_idx = rte_ctz32(hitmask) >> 1;
		if (buckets[bucket_id].sets[hit_idx] != RTE_MEMBER_NO_MATCH) {
			set_id[*counter] = buckets[bucket_id].sets[hit_idx];
			(*counter)++;
			if (*counter >= match_per_key)
				return;
		}
		hitmask &= ~(3U << ((hit_idx) << 1));
	}
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MEMBER_X86_H_ */
