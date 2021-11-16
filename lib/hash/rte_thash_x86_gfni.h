/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#ifndef _RTE_THASH_X86_GFNI_H_
#define _RTE_THASH_X86_GFNI_H_

/**
 * @file
 *
 * Optimized Toeplitz hash functions implementation
 * using Galois Fields New Instructions.
 */

#include <rte_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GFNI__) && defined(__AVX512F__)
#define RTE_THASH_GFNI_DEFINED

#define RTE_THASH_FIRST_ITER_MSK	0x0f0f0f0f0f0e0c08
#define RTE_THASH_PERM_MSK		0x0f0f0f0f0f0f0f0f
#define RTE_THASH_FIRST_ITER_MSK_2	0xf0f0f0f0f0e0c080
#define RTE_THASH_PERM_MSK_2		0xf0f0f0f0f0f0f0f0
#define RTE_THASH_REWIND_MSK		0x0000000000113377

__rte_internal
static inline void
__rte_thash_xor_reduce(__m512i xor_acc, uint32_t *val_1, uint32_t *val_2)
{
	__m256i tmp_256_1, tmp_256_2;
	__m128i tmp128_1, tmp128_2;

	tmp_256_1 = _mm512_castsi512_si256(xor_acc);
	tmp_256_2 = _mm512_extracti32x8_epi32(xor_acc, 1);
	tmp_256_1 = _mm256_xor_si256(tmp_256_1, tmp_256_2);

	tmp128_1 = _mm256_castsi256_si128(tmp_256_1);
	tmp128_2 = _mm256_extracti32x4_epi32(tmp_256_1, 1);
	tmp128_1 = _mm_xor_si128(tmp128_1, tmp128_2);

#ifdef RTE_ARCH_X86_64
	uint64_t tmp_1, tmp_2;
	tmp_1 = _mm_extract_epi64(tmp128_1, 0);
	tmp_2 = _mm_extract_epi64(tmp128_1, 1);
	tmp_1 ^= tmp_2;

	*val_1 = (uint32_t)tmp_1;
	*val_2 = (uint32_t)(tmp_1 >> 32);
#else
	uint32_t tmp_1, tmp_2;
	tmp_1 = _mm_extract_epi32(tmp128_1, 0);
	tmp_2 = _mm_extract_epi32(tmp128_1, 1);
	tmp_1 ^= _mm_extract_epi32(tmp128_1, 2);
	tmp_2 ^= _mm_extract_epi32(tmp128_1, 3);

	*val_1 = tmp_1;
	*val_2 = tmp_2;
#endif
}

__rte_internal
static inline __m512i
__rte_thash_gfni(const uint64_t *mtrx, const uint8_t *tuple,
	const uint8_t *secondary_tuple, int len)
{
	__m512i permute_idx = _mm512_set_epi32(0x07060504, 0x07060504,
		0x06050403, 0x06050403,
		0x05040302, 0x05040302,
		0x04030201, 0x04030201,
		0x03020100, 0x03020100,
		0x020100FF, 0x020100FF,
		0x0100FFFE, 0x0100FFFE,
		0x00FFFEFD, 0x00FFFEFD);
	const __m512i rewind_idx = _mm512_set_epi32(0x00000000, 0x00000000,
		0x00000000, 0x00000000,
		0x00000000, 0x00000000,
		0x00000000, 0x00000000,
		0x00000000, 0x00000000,
		0x0000003B, 0x0000003B,
		0x00003B3A, 0x00003B3A,
		0x003B3A39, 0x003B3A39);
	const __mmask64 rewind_mask = RTE_THASH_REWIND_MSK;
	const __m512i shift_8 = _mm512_set1_epi8(8);
	__m512i xor_acc = _mm512_setzero_si512();
	__m512i perm_bytes = _mm512_setzero_si512();
	__m512i vals, matrixes, tuple_bytes, tuple_bytes_2;
	__mmask64 load_mask, permute_mask, permute_mask_2;
	int chunk_len = 0, i = 0;
	uint8_t mtrx_msk;
	const int prepend = 3;

	for (; len > 0; len -= 64, tuple += 64) {
		if (i == 8)
			perm_bytes = _mm512_maskz_permutexvar_epi8(rewind_mask,
				rewind_idx, perm_bytes);

		permute_mask = RTE_THASH_FIRST_ITER_MSK;
		load_mask = (len >= 64) ? UINT64_MAX : ((1ULL << len) - 1);
		tuple_bytes = _mm512_maskz_loadu_epi8(load_mask, tuple);
		if (secondary_tuple) {
			permute_mask_2 = RTE_THASH_FIRST_ITER_MSK_2;
			tuple_bytes_2 = _mm512_maskz_loadu_epi8(load_mask,
				secondary_tuple);
		}

		chunk_len = __builtin_popcountll(load_mask);
		for (i = 0; i < ((chunk_len + prepend) / 8); i++, mtrx += 8) {
			perm_bytes = _mm512_mask_permutexvar_epi8(perm_bytes,
				permute_mask, permute_idx, tuple_bytes);

			if (secondary_tuple)
				perm_bytes =
					_mm512_mask_permutexvar_epi8(perm_bytes,
					permute_mask_2, permute_idx,
					tuple_bytes_2);

			matrixes = _mm512_maskz_loadu_epi64(UINT8_MAX, mtrx);
			vals = _mm512_gf2p8affine_epi64_epi8(perm_bytes,
				matrixes, 0);

			xor_acc = _mm512_xor_si512(xor_acc, vals);
			permute_idx = _mm512_add_epi8(permute_idx, shift_8);
			permute_mask = RTE_THASH_PERM_MSK;
			if (secondary_tuple)
				permute_mask_2 = RTE_THASH_PERM_MSK_2;
		}
	}

	int rest_len = (chunk_len + prepend) % 8;
	if (rest_len != 0) {
		mtrx_msk = (1 << (rest_len % 8)) - 1;
		matrixes = _mm512_maskz_loadu_epi64(mtrx_msk, mtrx);
		if (i == 8) {
			perm_bytes = _mm512_maskz_permutexvar_epi8(rewind_mask,
				rewind_idx, perm_bytes);
		} else {
			perm_bytes = _mm512_mask_permutexvar_epi8(perm_bytes,
				permute_mask, permute_idx, tuple_bytes);

			if (secondary_tuple)
				perm_bytes =
					_mm512_mask_permutexvar_epi8(
					perm_bytes, permute_mask_2,
					permute_idx, tuple_bytes_2);
		}

		vals = _mm512_gf2p8affine_epi64_epi8(perm_bytes, matrixes, 0);
		xor_acc = _mm512_xor_si512(xor_acc, vals);
	}

	return xor_acc;
}

/**
 * Calculate Toeplitz hash.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param m
 *  Pointer to the matrices generated from the corresponding
 *  RSS hash key using rte_thash_complete_matrix().
 *  Note that @p len should not exceed the length of the rss_key minus 4.
 * @param tuple
 *  Pointer to the data to be hashed. Data must be in network byte order.
 * @param len
 *  Length of the data to be hashed.
 * @return
 *  Calculated Toeplitz hash value.
 */
__rte_experimental
static inline uint32_t
rte_thash_gfni(const uint64_t *m, const uint8_t *tuple, int len)
{
	uint32_t val, val_zero;

	__m512i xor_acc = __rte_thash_gfni(m, tuple, NULL, len);
	__rte_thash_xor_reduce(xor_acc, &val, &val_zero);

	return val;
}

/**
 * Bulk implementation for Toeplitz hash.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param m
 *  Pointer to the matrices generated from the corresponding
 *  RSS hash key using rte_thash_complete_matrix().
 *  Note that @p len should not exceed the length of the rss_key minus 4.
 * @param len
 *  Length of the largest data buffer to be hashed.
 * @param tuple
 *  Array of the pointers on data to be hashed.
 *  Data must be in network byte order.
 * @param val
 *  Array of uint32_t where to put calculated Toeplitz hash values
 * @param num
 *  Number of tuples to hash.
 */
__rte_experimental
static inline void
rte_thash_gfni_bulk(const uint64_t *mtrx, int len, uint8_t *tuple[],
	uint32_t val[], uint32_t num)
{
	uint32_t i;
	uint32_t val_zero;
	__m512i xor_acc;

	for (i = 0; i != (num & ~1); i += 2) {
		xor_acc = __rte_thash_gfni(mtrx, tuple[i], tuple[i + 1], len);
		__rte_thash_xor_reduce(xor_acc, val + i, val + i + 1);
	}

	if (num & 1) {
		xor_acc = __rte_thash_gfni(mtrx, tuple[i], NULL, len);
		__rte_thash_xor_reduce(xor_acc, val + i, &val_zero);
	}
}

#endif /* __GFNI__ && __AVX512F__ */

#ifdef __cplusplus
}
#endif

#endif /* _RTE_THASH_X86_GFNI_H_ */
