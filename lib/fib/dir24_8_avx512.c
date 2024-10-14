/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <rte_vect.h>
#include <rte_fib.h>

#include "dir24_8.h"
#include "dir24_8_avx512.h"

static __rte_always_inline void
dir24_8_vec_lookup_x16(void *p, const uint32_t *ips,
	uint64_t *next_hops, int size, bool be_addr)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;
	__mmask16 msk_ext;
	__mmask16 exp_msk = 0x5555;
	__m512i ip_vec, idxes, res, bytes;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsb = _mm512_set1_epi32(1);
	const __m512i lsbyte_msk = _mm512_set1_epi32(0xff);
	__m512i tmp1, tmp2, res_msk;
	__m256i tmp256;
	/* used to mask gather values if size is 1/2 (8/16 bit next hops) */
	if (size == sizeof(uint8_t))
		res_msk = _mm512_set1_epi32(UINT8_MAX);
	else if (size == sizeof(uint16_t))
		res_msk = _mm512_set1_epi32(UINT16_MAX);

	ip_vec = _mm512_loadu_si512(ips);
	if (be_addr) {
		const __m512i bswap32 = _mm512_set_epi32(
			0x0c0d0e0f, 0x08090a0b, 0x04050607, 0x00010203,
			0x0c0d0e0f, 0x08090a0b, 0x04050607, 0x00010203,
			0x0c0d0e0f, 0x08090a0b, 0x04050607, 0x00010203,
			0x0c0d0e0f, 0x08090a0b, 0x04050607, 0x00010203
		);
		ip_vec = _mm512_shuffle_epi8(ip_vec, bswap32);
	}

	/* mask 24 most significant bits */
	idxes = _mm512_srli_epi32(ip_vec, 8);

	/**
	 * lookup in tbl24
	 * Put it inside branch to make compiler happy with -O0
	 */
	if (size == sizeof(uint8_t)) {
		res = _mm512_i32gather_epi32(idxes, (const int *)dp->tbl24, 1);
		res = _mm512_and_epi32(res, res_msk);
	} else if (size == sizeof(uint16_t)) {
		res = _mm512_i32gather_epi32(idxes, (const int *)dp->tbl24, 2);
		res = _mm512_and_epi32(res, res_msk);
	} else
		res = _mm512_i32gather_epi32(idxes, (const int *)dp->tbl24, 4);

	/* get extended entries indexes */
	msk_ext = _mm512_test_epi32_mask(res, lsb);

	if (msk_ext != 0) {
		idxes = _mm512_srli_epi32(res, 1);
		idxes = _mm512_slli_epi32(idxes, 8);
		bytes = _mm512_and_epi32(ip_vec, lsbyte_msk);
		idxes = _mm512_maskz_add_epi32(msk_ext, idxes, bytes);
		if (size == sizeof(uint8_t)) {
			idxes = _mm512_mask_i32gather_epi32(zero, msk_ext,
				idxes, (const int *)dp->tbl8, 1);
			idxes = _mm512_and_epi32(idxes, res_msk);
		} else if (size == sizeof(uint16_t)) {
			idxes = _mm512_mask_i32gather_epi32(zero, msk_ext,
				idxes, (const int *)dp->tbl8, 2);
			idxes = _mm512_and_epi32(idxes, res_msk);
		} else
			idxes = _mm512_mask_i32gather_epi32(zero, msk_ext,
				idxes, (const int *)dp->tbl8, 4);

		res = _mm512_mask_blend_epi32(msk_ext, res, idxes);
	}

	res = _mm512_srli_epi32(res, 1);
	tmp1 = _mm512_maskz_expand_epi32(exp_msk, res);
	tmp256 = _mm512_extracti32x8_epi32(res, 1);
	tmp2 = _mm512_maskz_expand_epi32(exp_msk,
		_mm512_castsi256_si512(tmp256));
	_mm512_storeu_si512(next_hops, tmp1);
	_mm512_storeu_si512(next_hops + 8, tmp2);
}

static __rte_always_inline void
dir24_8_vec_lookup_x8_8b(void *p, const uint32_t *ips,
	uint64_t *next_hops, bool be_addr)
{
	struct dir24_8_tbl *dp = (struct dir24_8_tbl *)p;
	const __m512i zero = _mm512_set1_epi32(0);
	const __m512i lsbyte_msk = _mm512_set1_epi64(0xff);
	const __m512i lsb = _mm512_set1_epi64(1);
	__m512i res, idxes, bytes;
	__m256i idxes_256, ip_vec;
	__mmask8 msk_ext;

	ip_vec = _mm256_loadu_si256((const void *)ips);
	if (be_addr) {
		const __m256i bswap32 = _mm256_set_epi8(
			12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
			12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3
		);
		ip_vec = _mm256_shuffle_epi8(ip_vec, bswap32);
	}
	/* mask 24 most significant bits */
	idxes_256 = _mm256_srli_epi32(ip_vec, 8);

	/* lookup in tbl24 */
	res = _mm512_i32gather_epi64(idxes_256, (const void *)dp->tbl24, 8);

	/* get extended entries indexes */
	msk_ext = _mm512_test_epi64_mask(res, lsb);

	if (msk_ext != 0) {
		bytes = _mm512_cvtepi32_epi64(ip_vec);
		idxes = _mm512_srli_epi64(res, 1);
		idxes = _mm512_slli_epi64(idxes, 8);
		bytes = _mm512_and_epi64(bytes, lsbyte_msk);
		idxes = _mm512_maskz_add_epi64(msk_ext, idxes, bytes);
		idxes = _mm512_mask_i64gather_epi64(zero, msk_ext, idxes,
			(const void *)dp->tbl8, 8);

		res = _mm512_mask_blend_epi64(msk_ext, res, idxes);
	}

	res = _mm512_srli_epi64(res, 1);
	_mm512_storeu_si512(next_hops, res);
}

#define DECLARE_VECTOR_FN(suffix, nh_type, be_addr) \
void \
rte_dir24_8_vec_lookup_bulk_##suffix(void *p, const uint32_t *ips, uint64_t *next_hops, \
	const unsigned int n) \
{ \
	uint32_t i; \
	for (i = 0; i < (n / 16); i++) \
		dir24_8_vec_lookup_x16(p, ips + i * 16, next_hops + i * 16, sizeof(nh_type), \
			be_addr); \
	dir24_8_lookup_bulk_##suffix(p, ips + i * 16, next_hops + i * 16, n - i * 16); \
}

DECLARE_VECTOR_FN(1b, uint8_t, false)
DECLARE_VECTOR_FN(1b_be, uint8_t, true)
DECLARE_VECTOR_FN(2b, uint16_t, false)
DECLARE_VECTOR_FN(2b_be, uint16_t, true)
DECLARE_VECTOR_FN(4b, uint32_t, false)
DECLARE_VECTOR_FN(4b_be, uint32_t, true)

void
rte_dir24_8_vec_lookup_bulk_8b(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 8); i++)
		dir24_8_vec_lookup_x8_8b(p, ips + i * 8, next_hops + i * 8, false);
	dir24_8_lookup_bulk_8b(p, ips + i * 8, next_hops + i * 8, n - i * 8);
}

void
rte_dir24_8_vec_lookup_bulk_8b_be(void *p, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n)
{
	uint32_t i;
	for (i = 0; i < (n / 8); i++)
		dir24_8_vec_lookup_x8_8b(p, ips + i * 8, next_hops + i * 8, true);
	dir24_8_lookup_bulk_8b_be(p, ips + i * 8, next_hops + i * 8, n - i * 8);
}
