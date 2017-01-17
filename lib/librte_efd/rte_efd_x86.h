/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

/* rte_efd_x86.h
 * This file holds all x86 specific EFD functions
 */
#include <immintrin.h>

#if (RTE_EFD_VALUE_NUM_BITS == 8 || RTE_EFD_VALUE_NUM_BITS == 16 || \
	RTE_EFD_VALUE_NUM_BITS == 24 || RTE_EFD_VALUE_NUM_BITS == 32)
#define EFD_LOAD_SI128(val) _mm_load_si128(val)
#else
#define EFD_LOAD_SI128(val) _mm_lddqu_si128(val)
#endif

static inline efd_value_t
efd_lookup_internal_avx2(const efd_hashfunc_t *group_hash_idx,
		const efd_lookuptbl_t *group_lookup_table,
		const uint32_t hash_val_a, const uint32_t hash_val_b)
{
#ifdef RTE_MACHINE_CPUFLAG_AVX2
	efd_value_t value = 0;
	uint32_t i = 0;
	__m256i vhash_val_a = _mm256_set1_epi32(hash_val_a);
	__m256i vhash_val_b = _mm256_set1_epi32(hash_val_b);

	for (; i < RTE_EFD_VALUE_NUM_BITS; i += 8) {
		__m256i vhash_idx =
				_mm256_cvtepu16_epi32(EFD_LOAD_SI128(
				(__m128i const *) &group_hash_idx[i]));
		__m256i vlookup_table = _mm256_cvtepu16_epi32(
				EFD_LOAD_SI128((__m128i const *)
				&group_lookup_table[i]));
		__m256i vhash = _mm256_add_epi32(vhash_val_a,
				_mm256_mullo_epi32(vhash_idx, vhash_val_b));
		__m256i vbucket_idx = _mm256_srli_epi32(vhash,
				EFD_LOOKUPTBL_SHIFT);
		__m256i vresult = _mm256_srlv_epi32(vlookup_table,
				vbucket_idx);

		value |= (_mm256_movemask_ps(
			(__m256) _mm256_slli_epi32(vresult, 31))
			& ((1 << (RTE_EFD_VALUE_NUM_BITS - i)) - 1)) << i;
	}

	return value;
#else
	RTE_SET_USED(group_hash_idx);
	RTE_SET_USED(group_lookup_table);
	RTE_SET_USED(hash_val_a);
	RTE_SET_USED(hash_val_b);
	/* Return dummy value, only to avoid compilation breakage */
	return 0;
#endif

}
