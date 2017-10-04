/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_MEMBER_X86_H_
#define _RTE_MEMBER_X86_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <x86intrin.h>

#if defined(RTE_MACHINE_CPUFLAG_AVX2)

static inline int
update_entry_search_avx(uint32_t bucket_id, member_sig_t tmp_sig,
		struct member_ht_bucket *buckets,
		member_set_t set_id)
{
	uint32_t hitmask = _mm256_movemask_epi8((__m256i)_mm256_cmpeq_epi16(
		_mm256_load_si256((__m256i const *)buckets[bucket_id].sigs),
		_mm256_set1_epi16(tmp_sig)));
	if (hitmask) {
		uint32_t hit_idx = __builtin_ctzl(hitmask) >> 1;
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
		uint32_t hit_idx = __builtin_ctzl(hitmask) >> 1;
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
		uint32_t hit_idx = __builtin_ctzl(hitmask) >> 1;
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
