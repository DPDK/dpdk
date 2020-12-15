/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_vect.h>
#include <rte_fib6.h>

#include "trie.h"
#include "trie_avx512.h"

static __rte_always_inline void
transpose_x16(uint8_t ips[16][RTE_FIB6_IPV6_ADDR_SIZE],
	__m512i *first, __m512i *second, __m512i *third, __m512i *fourth)
{
	__m512i tmp1, tmp2, tmp3, tmp4;
	__m512i tmp5, tmp6, tmp7, tmp8;
	const __rte_x86_zmm_t perm_idxes = {
		.u32 = { 0, 4, 8, 12, 2, 6, 10, 14,
			1, 5, 9, 13, 3, 7, 11, 15
		},
	};

	/* load all ip addresses */
	tmp1 = _mm512_loadu_si512(&ips[0][0]);
	tmp2 = _mm512_loadu_si512(&ips[4][0]);
	tmp3 = _mm512_loadu_si512(&ips[8][0]);
	tmp4 = _mm512_loadu_si512(&ips[12][0]);

	/* transpose 4 byte chunks of 16 ips */
	tmp5 = _mm512_unpacklo_epi32(tmp1, tmp2);
	tmp7 = _mm512_unpackhi_epi32(tmp1, tmp2);
	tmp6 = _mm512_unpacklo_epi32(tmp3, tmp4);
	tmp8 = _mm512_unpackhi_epi32(tmp3, tmp4);

	tmp1 = _mm512_unpacklo_epi32(tmp5, tmp6);
	tmp3 = _mm512_unpackhi_epi32(tmp5, tmp6);
	tmp2 = _mm512_unpacklo_epi32(tmp7, tmp8);
	tmp4 = _mm512_unpackhi_epi32(tmp7, tmp8);

	/* first 4-byte chunks of ips[] */
	*first = _mm512_permutexvar_epi32(perm_idxes.z, tmp1);
	/* second 4-byte chunks of ips[] */
	*second = _mm512_permutexvar_epi32(perm_idxes.z, tmp3);
	/* third 4-byte chunks of ips[] */
	*third = _mm512_permutexvar_epi32(perm_idxes.z, tmp2);
	/* fourth 4-byte chunks of ips[] */
	*fourth = _mm512_permutexvar_epi32(perm_idxes.z, tmp4);
}

static __rte_always_inline void
transpose_x8(uint8_t ips[8][RTE_FIB6_IPV6_ADDR_SIZE],
	__m512i *first, __m512i *second)
{
	__m512i tmp1, tmp2, tmp3, tmp4;
	const __rte_x86_zmm_t perm_idxes = {
		.u64 = { 0, 2, 4, 6, 1, 3, 5, 7
		},
	};

	tmp1 = _mm512_loadu_si512(&ips[0][0]);
	tmp2 = _mm512_loadu_si512(&ips[4][0]);

	tmp3 = _mm512_unpacklo_epi64(tmp1, tmp2);
	*first = _mm512_permutexvar_epi64(perm_idxes.z, tmp3);
	tmp4 = _mm512_unpackhi_epi64(tmp1, tmp2);
	*second = _mm512_permutexvar_epi64(perm_idxes.z, tmp4);
}

static __rte_always_inline void
trie_vec_lookup_x16x2(void *p, uint8_t ips[32][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, int size)
{
	struct rte_trie_tbl *dp = (struct rte_trie_tbl *)p;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsb = _mm512_set1_epi32(1);
	const __m512i two_lsb = _mm512_set1_epi32(3);
	/* IPv6 four byte chunks */
	__m512i first_1, second_1, third_1, fourth_1;
	__m512i first_2, second_2, third_2, fourth_2;
	__m512i idxes_1, res_1;
	__m512i idxes_2, res_2;
	__m512i shuf_idxes;
	__m512i tmp_1, tmp2_1, bytes_1, byte_chunk_1;
	__m512i tmp_2, tmp2_2, bytes_2, byte_chunk_2;
	__m512i base_idxes;
	/* used to mask gather values if size is 2 (16 bit next hops) */
	const __m512i res_msk = _mm512_set1_epi32(UINT16_MAX);
	const __rte_x86_zmm_t bswap = {
		.u8 = { 2, 1, 0, 255, 6, 5, 4, 255,
			10, 9, 8, 255, 14, 13, 12, 255,
			2, 1, 0, 255, 6, 5, 4, 255,
			10, 9, 8, 255, 14, 13, 12, 255,
			2, 1, 0, 255, 6, 5, 4, 255,
			10, 9, 8, 255, 14, 13, 12, 255,
			2, 1, 0, 255, 6, 5, 4, 255,
			10, 9, 8, 255, 14, 13, 12, 255
			},
	};
	const __mmask64 k = 0x1111111111111111;
	int i = 3;
	__mmask16 msk_ext_1, new_msk_1;
	__mmask16 msk_ext_2, new_msk_2;
	__mmask16 exp_msk = 0x5555;

	transpose_x16(ips, &first_1, &second_1, &third_1, &fourth_1);
	transpose_x16(ips + 16, &first_2, &second_2, &third_2, &fourth_2);

	/* get_tbl24_idx() for every 4 byte chunk */
	idxes_1 = _mm512_shuffle_epi8(first_1, bswap.z);
	idxes_2 = _mm512_shuffle_epi8(first_2, bswap.z);

	/**
	 * lookup in tbl24
	 * Put it inside branch to make compiller happy with -O0
	 */
	if (size == sizeof(uint16_t)) {
		res_1 = _mm512_i32gather_epi32(idxes_1,
				(const int *)dp->tbl24, 2);
		res_2 = _mm512_i32gather_epi32(idxes_2,
				(const int *)dp->tbl24, 2);
		res_1 = _mm512_and_epi32(res_1, res_msk);
		res_2 = _mm512_and_epi32(res_2, res_msk);
	} else {
		res_1 = _mm512_i32gather_epi32(idxes_1,
				(const int *)dp->tbl24, 4);
		res_2 = _mm512_i32gather_epi32(idxes_2,
				(const int *)dp->tbl24, 4);
	}

	/* get extended entries indexes */
	msk_ext_1 = _mm512_test_epi32_mask(res_1, lsb);
	msk_ext_2 = _mm512_test_epi32_mask(res_2, lsb);

	tmp_1 = _mm512_srli_epi32(res_1, 1);
	tmp_2 = _mm512_srli_epi32(res_2, 1);

	/* idxes to retrieve bytes */
	shuf_idxes = _mm512_setr_epi32(3, 7, 11, 15,
				19, 23, 27, 31,
				35, 39, 43, 47,
				51, 55, 59, 63);

	base_idxes = _mm512_setr_epi32(0, 4, 8, 12,
				16, 20, 24, 28,
				32, 36, 40, 44,
				48, 52, 56, 60);

	/* traverse down the trie */
	while (msk_ext_1 || msk_ext_2) {
		idxes_1 = _mm512_maskz_slli_epi32(msk_ext_1, tmp_1, 8);
		idxes_2 = _mm512_maskz_slli_epi32(msk_ext_2, tmp_2, 8);
		byte_chunk_1 = (i < 8) ?
			((i >= 4) ? second_1 : first_1) :
			((i >= 12) ? fourth_1 : third_1);
		byte_chunk_2 = (i < 8) ?
			((i >= 4) ? second_2 : first_2) :
			((i >= 12) ? fourth_2 : third_2);
		bytes_1 = _mm512_maskz_shuffle_epi8(k, byte_chunk_1,
				shuf_idxes);
		bytes_2 = _mm512_maskz_shuffle_epi8(k, byte_chunk_2,
				shuf_idxes);
		idxes_1 = _mm512_maskz_add_epi32(msk_ext_1, idxes_1, bytes_1);
		idxes_2 = _mm512_maskz_add_epi32(msk_ext_2, idxes_2, bytes_2);
		if (size == sizeof(uint16_t)) {
			tmp_1 = _mm512_mask_i32gather_epi32(zero, msk_ext_1,
				idxes_1, (const int *)dp->tbl8, 2);
			tmp_2 = _mm512_mask_i32gather_epi32(zero, msk_ext_2,
				idxes_2, (const int *)dp->tbl8, 2);
			tmp_1 = _mm512_and_epi32(tmp_1, res_msk);
			tmp_2 = _mm512_and_epi32(tmp_2, res_msk);
		} else {
			tmp_1 = _mm512_mask_i32gather_epi32(zero, msk_ext_1,
				idxes_1, (const int *)dp->tbl8, 4);
			tmp_2 = _mm512_mask_i32gather_epi32(zero, msk_ext_2,
				idxes_2, (const int *)dp->tbl8, 4);
		}
		new_msk_1 = _mm512_test_epi32_mask(tmp_1, lsb);
		new_msk_2 = _mm512_test_epi32_mask(tmp_2, lsb);
		res_1 = _mm512_mask_blend_epi32(msk_ext_1 ^ new_msk_1, res_1,
				tmp_1);
		res_2 = _mm512_mask_blend_epi32(msk_ext_2 ^ new_msk_2, res_2,
				tmp_2);
		tmp_1 = _mm512_srli_epi32(tmp_1, 1);
		tmp_2 = _mm512_srli_epi32(tmp_2, 1);
		msk_ext_1 = new_msk_1;
		msk_ext_2 = new_msk_2;

		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, lsb);
		shuf_idxes = _mm512_and_epi32(shuf_idxes, two_lsb);
		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, base_idxes);
		i++;
	}

	/* get rid of 1 LSB, now we have HN in every epi32 */
	res_1 = _mm512_srli_epi32(res_1, 1);
	res_2 = _mm512_srli_epi32(res_2, 1);
	/* extract first half of NH's each in epi64 chunk */
	tmp_1 = _mm512_maskz_expand_epi32(exp_msk, res_1);
	tmp_2 = _mm512_maskz_expand_epi32(exp_msk, res_2);
	/* extract second half of NH's */
	__m256i tmp256_1, tmp256_2;
	tmp256_1 = _mm512_extracti32x8_epi32(res_1, 1);
	tmp256_2 = _mm512_extracti32x8_epi32(res_2, 1);
	tmp2_1 = _mm512_maskz_expand_epi32(exp_msk,
		_mm512_castsi256_si512(tmp256_1));
	tmp2_2 = _mm512_maskz_expand_epi32(exp_msk,
		_mm512_castsi256_si512(tmp256_2));
	/* return NH's from two sets of registers */
	_mm512_storeu_si512(next_hops, tmp_1);
	_mm512_storeu_si512(next_hops + 8, tmp2_1);
	_mm512_storeu_si512(next_hops + 16, tmp_2);
	_mm512_storeu_si512(next_hops + 24, tmp2_2);
}

static void
trie_vec_lookup_x8x2_8b(void *p, uint8_t ips[16][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops)
{
	struct rte_trie_tbl *dp = (struct rte_trie_tbl *)p;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsb = _mm512_set1_epi32(1);
	const __m512i three_lsb = _mm512_set1_epi32(7);
	/* IPv6 eight byte chunks */
	__m512i first_1, second_1;
	__m512i first_2, second_2;
	__m512i idxes_1, res_1;
	__m512i idxes_2, res_2;
	__m512i shuf_idxes, base_idxes;
	__m512i tmp_1, bytes_1, byte_chunk_1;
	__m512i tmp_2, bytes_2, byte_chunk_2;
	const __rte_x86_zmm_t bswap = {
		.u8 = { 2, 1, 0, 255, 255, 255, 255, 255,
			10, 9, 8, 255, 255, 255, 255, 255,
			2, 1, 0, 255, 255, 255, 255, 255,
			10, 9, 8, 255, 255, 255, 255, 255,
			2, 1, 0, 255, 255, 255, 255, 255,
			10, 9, 8, 255, 255, 255, 255, 255,
			2, 1, 0, 255, 255, 255, 255, 255,
			10, 9, 8, 255, 255, 255, 255, 255
			},
	};
	const __mmask64 k = 0x101010101010101;
	int i = 3;
	__mmask8 msk_ext_1, new_msk_1;
	__mmask8 msk_ext_2, new_msk_2;

	transpose_x8(ips, &first_1, &second_1);
	transpose_x8(ips + 8, &first_2, &second_2);

	/* get_tbl24_idx() for every 4 byte chunk */
	idxes_1 = _mm512_shuffle_epi8(first_1, bswap.z);
	idxes_2 = _mm512_shuffle_epi8(first_2, bswap.z);

	/* lookup in tbl24 */
	res_1 = _mm512_i64gather_epi64(idxes_1, (const void *)dp->tbl24, 8);
	res_2 = _mm512_i64gather_epi64(idxes_2, (const void *)dp->tbl24, 8);
	/* get extended entries indexes */
	msk_ext_1 = _mm512_test_epi64_mask(res_1, lsb);
	msk_ext_2 = _mm512_test_epi64_mask(res_2, lsb);

	tmp_1 = _mm512_srli_epi64(res_1, 1);
	tmp_2 = _mm512_srli_epi64(res_2, 1);

	/* idxes to retrieve bytes */
	shuf_idxes = _mm512_setr_epi64(3, 11, 19, 27, 35, 43, 51, 59);

	base_idxes = _mm512_setr_epi64(0, 8, 16, 24, 32, 40, 48, 56);

	/* traverse down the trie */
	while (msk_ext_1 || msk_ext_2) {
		idxes_1 = _mm512_maskz_slli_epi64(msk_ext_1, tmp_1, 8);
		idxes_2 = _mm512_maskz_slli_epi64(msk_ext_2, tmp_2, 8);
		byte_chunk_1 = (i < 8) ? first_1 : second_1;
		byte_chunk_2 = (i < 8) ? first_2 : second_2;
		bytes_1 = _mm512_maskz_shuffle_epi8(k, byte_chunk_1,
				shuf_idxes);
		bytes_2 = _mm512_maskz_shuffle_epi8(k, byte_chunk_2,
				shuf_idxes);
		idxes_1 = _mm512_maskz_add_epi64(msk_ext_1, idxes_1, bytes_1);
		idxes_2 = _mm512_maskz_add_epi64(msk_ext_2, idxes_2, bytes_2);
		tmp_1 = _mm512_mask_i64gather_epi64(zero, msk_ext_1,
				idxes_1, (const void *)dp->tbl8, 8);
		tmp_2 = _mm512_mask_i64gather_epi64(zero, msk_ext_2,
				idxes_2, (const void *)dp->tbl8, 8);
		new_msk_1 = _mm512_test_epi64_mask(tmp_1, lsb);
		new_msk_2 = _mm512_test_epi64_mask(tmp_2, lsb);
		res_1 = _mm512_mask_blend_epi64(msk_ext_1 ^ new_msk_1, res_1,
				tmp_1);
		res_2 = _mm512_mask_blend_epi64(msk_ext_2 ^ new_msk_2, res_2,
				tmp_2);
		tmp_1 = _mm512_srli_epi64(tmp_1, 1);
		tmp_2 = _mm512_srli_epi64(tmp_2, 1);
		msk_ext_1 = new_msk_1;
		msk_ext_2 = new_msk_2;

		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, lsb);
		shuf_idxes = _mm512_and_epi64(shuf_idxes, three_lsb);
		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, base_idxes);
		i++;
	}

	res_1 = _mm512_srli_epi64(res_1, 1);
	res_2 = _mm512_srli_epi64(res_2, 1);
	_mm512_storeu_si512(next_hops, res_1);
	_mm512_storeu_si512(next_hops + 8, res_2);
}

void
rte_trie_vec_lookup_bulk_2b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 32); i++) {
		trie_vec_lookup_x16x2(p, (uint8_t (*)[16])&ips[i * 32][0],
				next_hops + i * 32, sizeof(uint16_t));
	}
	rte_trie_lookup_bulk_2b(p, (uint8_t (*)[16])&ips[i * 32][0],
			next_hops + i * 32, n - i * 32);
}

void
rte_trie_vec_lookup_bulk_4b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 32); i++) {
		trie_vec_lookup_x16x2(p, (uint8_t (*)[16])&ips[i * 32][0],
				next_hops + i * 32, sizeof(uint32_t));
	}
	rte_trie_lookup_bulk_4b(p, (uint8_t (*)[16])&ips[i * 32][0],
			next_hops + i * 32, n - i * 32);
}

void
rte_trie_vec_lookup_bulk_8b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 16); i++) {
		trie_vec_lookup_x8x2_8b(p, (uint8_t (*)[16])&ips[i * 16][0],
				next_hops + i * 16);
	}
	rte_trie_lookup_bulk_8b(p, (uint8_t (*)[16])&ips[i * 16][0],
			next_hops + i * 16, n - i * 16);
}
