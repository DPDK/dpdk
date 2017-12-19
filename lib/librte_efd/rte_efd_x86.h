/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
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
