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
trie_vec_lookup_x16(void *p, uint8_t ips[16][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, int size)
{
	struct rte_trie_tbl *dp = (struct rte_trie_tbl *)p;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsb = _mm512_set1_epi32(1);
	const __m512i two_lsb = _mm512_set1_epi32(3);
	__m512i first, second, third, fourth; /*< IPv6 four byte chunks */
	__m512i idxes, res, shuf_idxes;
	__m512i tmp, tmp2, bytes, byte_chunk, base_idxes;
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
	__mmask16 msk_ext, new_msk;
	__mmask16 exp_msk = 0x5555;

	transpose_x16(ips, &first, &second, &third, &fourth);

	/* get_tbl24_idx() for every 4 byte chunk */
	idxes = _mm512_shuffle_epi8(first, bswap.z);

	/**
	 * lookup in tbl24
	 * Put it inside branch to make compiller happy with -O0
	 */
	if (size == sizeof(uint16_t)) {
		res = _mm512_i32gather_epi32(idxes, (const int *)dp->tbl24, 2);
		res = _mm512_and_epi32(res, res_msk);
	} else
		res = _mm512_i32gather_epi32(idxes, (const int *)dp->tbl24, 4);


	/* get extended entries indexes */
	msk_ext = _mm512_test_epi32_mask(res, lsb);

	tmp = _mm512_srli_epi32(res, 1);

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
	while (msk_ext) {
		idxes = _mm512_maskz_slli_epi32(msk_ext, tmp, 8);
		byte_chunk = (i < 8) ?
			((i >= 4) ? second : first) :
			((i >= 12) ? fourth : third);
		bytes = _mm512_maskz_shuffle_epi8(k, byte_chunk, shuf_idxes);
		idxes = _mm512_maskz_add_epi32(msk_ext, idxes, bytes);
		if (size == sizeof(uint16_t)) {
			tmp = _mm512_mask_i32gather_epi32(zero, msk_ext,
				idxes, (const int *)dp->tbl8, 2);
			tmp = _mm512_and_epi32(tmp, res_msk);
		} else
			tmp = _mm512_mask_i32gather_epi32(zero, msk_ext,
				idxes, (const int *)dp->tbl8, 4);
		new_msk = _mm512_test_epi32_mask(tmp, lsb);
		res = _mm512_mask_blend_epi32(msk_ext ^ new_msk, res, tmp);
		tmp = _mm512_srli_epi32(tmp, 1);
		msk_ext = new_msk;

		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, lsb);
		shuf_idxes = _mm512_and_epi32(shuf_idxes, two_lsb);
		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, base_idxes);
		i++;
	}

	res = _mm512_srli_epi32(res, 1);
	tmp = _mm512_maskz_expand_epi32(exp_msk, res);
	__m256i tmp256;
	tmp256 = _mm512_extracti32x8_epi32(res, 1);
	tmp2 = _mm512_maskz_expand_epi32(exp_msk,
		_mm512_castsi256_si512(tmp256));
	_mm512_storeu_si512(next_hops, tmp);
	_mm512_storeu_si512(next_hops + 8, tmp2);
}

static void
trie_vec_lookup_x8_8b(void *p, uint8_t ips[8][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops)
{
	struct rte_trie_tbl *dp = (struct rte_trie_tbl *)p;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsb = _mm512_set1_epi32(1);
	const __m512i three_lsb = _mm512_set1_epi32(7);
	__m512i first, second; /*< IPv6 eight byte chunks */
	__m512i idxes, res, shuf_idxes;
	__m512i tmp, bytes, byte_chunk, base_idxes;
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
	__mmask8 msk_ext, new_msk;

	transpose_x8(ips, &first, &second);

	/* get_tbl24_idx() for every 4 byte chunk */
	idxes = _mm512_shuffle_epi8(first, bswap.z);

	/* lookup in tbl24 */
	res = _mm512_i64gather_epi64(idxes, (const void *)dp->tbl24, 8);
	/* get extended entries indexes */
	msk_ext = _mm512_test_epi64_mask(res, lsb);

	tmp = _mm512_srli_epi64(res, 1);

	/* idxes to retrieve bytes */
	shuf_idxes = _mm512_setr_epi64(3, 11, 19, 27, 35, 43, 51, 59);

	base_idxes = _mm512_setr_epi64(0, 8, 16, 24, 32, 40, 48, 56);

	/* traverse down the trie */
	while (msk_ext) {
		idxes = _mm512_maskz_slli_epi64(msk_ext, tmp, 8);
		byte_chunk = (i < 8) ? first : second;
		bytes = _mm512_maskz_shuffle_epi8(k, byte_chunk, shuf_idxes);
		idxes = _mm512_maskz_add_epi64(msk_ext, idxes, bytes);
		tmp = _mm512_mask_i64gather_epi64(zero, msk_ext,
				idxes, (const void *)dp->tbl8, 8);
		new_msk = _mm512_test_epi64_mask(tmp, lsb);
		res = _mm512_mask_blend_epi64(msk_ext ^ new_msk, res, tmp);
		tmp = _mm512_srli_epi64(tmp, 1);
		msk_ext = new_msk;

		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, lsb);
		shuf_idxes = _mm512_and_epi64(shuf_idxes, three_lsb);
		shuf_idxes = _mm512_maskz_add_epi8(k, shuf_idxes, base_idxes);
		i++;
	}

	res = _mm512_srli_epi64(res, 1);
	_mm512_storeu_si512(next_hops, res);
}

void
rte_trie_vec_lookup_bulk_2b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 16); i++) {
		trie_vec_lookup_x16(p, (uint8_t (*)[16])&ips[i * 16][0],
				next_hops + i * 16, sizeof(uint16_t));
	}
	rte_trie_lookup_bulk_2b(p, (uint8_t (*)[16])&ips[i * 16][0],
			next_hops + i * 16, n - i * 16);
}

void
rte_trie_vec_lookup_bulk_4b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 16); i++) {
		trie_vec_lookup_x16(p, (uint8_t (*)[16])&ips[i * 16][0],
				next_hops + i * 16, sizeof(uint32_t));
	}
	rte_trie_lookup_bulk_4b(p, (uint8_t (*)[16])&ips[i * 16][0],
			next_hops + i * 16, n - i * 16);
}

void
rte_trie_vec_lookup_bulk_8b(void *p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 8); i++) {
		trie_vec_lookup_x8_8b(p, (uint8_t (*)[16])&ips[i * 8][0],
				next_hops + i * 8);
	}
	rte_trie_lookup_bulk_8b(p, (uint8_t (*)[16])&ips[i * 8][0],
			next_hops + i * 8, n - i * 8);
}
