/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */


#include <rte_common.h>

#include "net_crc.h"

#include <x86intrin.h>

/* VPCLMULQDQ CRC computation context structure */
struct crc_vpclmulqdq_ctx {
	__m512i rk1_rk2;
	__m512i rk3_rk4;
	__m512i fold_7x128b;
	__m512i fold_3x128b;
	__m128i rk5_rk6;
	__m128i rk7_rk8;
	__m128i fold_1x128b;
};

static struct crc_vpclmulqdq_ctx crc32_eth __rte_aligned(64);
static struct crc_vpclmulqdq_ctx crc16_ccitt __rte_aligned(64);

static uint16_t byte_len_to_mask_table[] = {
	0x0000, 0x0001, 0x0003, 0x0007,
	0x000f, 0x001f, 0x003f, 0x007f,
	0x00ff, 0x01ff, 0x03ff, 0x07ff,
	0x0fff, 0x1fff, 0x3fff, 0x7fff,
	0xffff};

static const uint8_t shf_table[32] __rte_aligned(16) = {
	0x00, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static const uint32_t mask[4] __rte_aligned(16) = {
	0xffffffff, 0xffffffff, 0x00000000, 0x00000000
};

static const uint32_t mask2[4] __rte_aligned(16) = {
	0x00000000, 0xffffffff, 0xffffffff, 0xffffffff
};

static __rte_always_inline __m512i
crcr32_folding_round(__m512i data_block, __m512i precomp, __m512i fold)
{
	__m512i tmp0, tmp1;

	tmp0 = _mm512_clmulepi64_epi128(fold, precomp, 0x01);
	tmp1 = _mm512_clmulepi64_epi128(fold, precomp, 0x10);

	return _mm512_ternarylogic_epi64(tmp0, tmp1, data_block, 0x96);
}

static __rte_always_inline __m128i
crc32_fold_128(__m512i fold0, __m512i fold1,
	const struct crc_vpclmulqdq_ctx *params)
{
	__m128i res, res2;
	__m256i a;
	__m512i tmp0, tmp1, tmp2, tmp3;
	__m512i tmp4;

	tmp0 = _mm512_clmulepi64_epi128(fold0, params->fold_7x128b, 0x01);
	tmp1 = _mm512_clmulepi64_epi128(fold0, params->fold_7x128b, 0x10);

	res = _mm512_extracti64x2_epi64(fold1, 3);
	tmp4 = _mm512_maskz_broadcast_i32x4(0xF, res);

	tmp2 = _mm512_clmulepi64_epi128(fold1, params->fold_3x128b, 0x01);
	tmp3 = _mm512_clmulepi64_epi128(fold1, params->fold_3x128b, 0x10);

	tmp0 = _mm512_ternarylogic_epi64(tmp0, tmp1, tmp2, 0x96);
	tmp0 = _mm512_ternarylogic_epi64(tmp0, tmp3, tmp4, 0x96);

	tmp1 = _mm512_shuffle_i64x2(tmp0, tmp0, 0x4e);

	a = _mm256_xor_si256(*(__m256i *)&tmp1, *(__m256i *)&tmp0);
	res = _mm256_extracti64x2_epi64(a, 1);
	res2 = _mm_xor_si128(res, *(__m128i *)&a);

	return res2;
}

static __rte_always_inline __m128i
last_two_xmm(const uint8_t *data, uint32_t data_len, uint32_t n, __m128i res,
	const struct crc_vpclmulqdq_ctx *params)
{
	uint32_t offset;
	__m128i res2, res3, res4, pshufb_shf;

	const uint32_t mask3[4] __rte_aligned(16) = {
		   0x80808080, 0x80808080, 0x80808080, 0x80808080
	};

	res2 = res;
	offset = data_len - n;
	res3 = _mm_loadu_si128((const __m128i *)&data[n+offset-16]);

	pshufb_shf = _mm_loadu_si128((const __m128i *)
			(shf_table + (data_len-n)));

	res = _mm_shuffle_epi8(res, pshufb_shf);
	pshufb_shf = _mm_xor_si128(pshufb_shf,
			_mm_load_si128((const __m128i *) mask3));
	res2 = _mm_shuffle_epi8(res2, pshufb_shf);

	res2 = _mm_blendv_epi8(res2, res3, pshufb_shf);

	res4 = _mm_clmulepi64_si128(res, params->fold_1x128b, 0x01);
	res = _mm_clmulepi64_si128(res, params->fold_1x128b, 0x10);
	res = _mm_ternarylogic_epi64(res, res2, res4, 0x96);

	return res;
}

static __rte_always_inline __m128i
done_128(__m128i res, const struct crc_vpclmulqdq_ctx *params)
{
	__m128i res1;

	res1 = res;

	res = _mm_clmulepi64_si128(res, params->rk5_rk6, 0x0);
	res1 = _mm_srli_si128(res1, 8);
	res = _mm_xor_si128(res, res1);

	res1 = res;
	res = _mm_slli_si128(res, 4);
	res = _mm_clmulepi64_si128(res, params->rk5_rk6, 0x10);
	res = _mm_xor_si128(res, res1);

	return res;
}

static __rte_always_inline uint32_t
barrett_reduction(__m128i data64, const struct crc_vpclmulqdq_ctx *params)
{
	__m128i tmp0, tmp1;

	data64 =  _mm_and_si128(data64, *(const __m128i *)mask2);
	tmp0 = data64;
	tmp1 = data64;

	data64 = _mm_clmulepi64_si128(tmp0, params->rk7_rk8, 0x0);
	data64 = _mm_ternarylogic_epi64(data64, tmp1, *(const __m128i *)mask,
			0x28);

	tmp1 = data64;
	data64 = _mm_clmulepi64_si128(data64, params->rk7_rk8, 0x10);
	data64 = _mm_ternarylogic_epi64(data64, tmp1, tmp0, 0x96);

	return _mm_extract_epi32(data64, 2);
}

static __rte_always_inline void
reduction_loop(__m128i *fold, int *len, const uint8_t *data, uint32_t *n,
	const struct crc_vpclmulqdq_ctx *params)
{
	__m128i tmp, tmp1;

	tmp = _mm_clmulepi64_si128(*fold, params->fold_1x128b, 0x1);
	*fold = _mm_clmulepi64_si128(*fold, params->fold_1x128b, 0x10);
	*fold = _mm_xor_si128(*fold, tmp);
	tmp1 = _mm_loadu_si128((const __m128i *)&data[*n]);
	*fold = _mm_xor_si128(*fold, tmp1);
	*n += 16;
	*len -= 16;
}

static __rte_always_inline uint32_t
crc32_eth_calc_vpclmulqdq(const uint8_t *data, uint32_t data_len, uint32_t crc,
	const struct crc_vpclmulqdq_ctx *params)
{
	__m128i res, d, b;
	__m512i temp, k;
	__m512i qw0 = _mm512_set1_epi64(0), qw1, qw2, qw3;
	__m512i fold0, fold1, fold2, fold3;
	__mmask16 mask;
	uint32_t n = 0;
	int reduction = 0;

	/* Get CRC init value */
	b = _mm_cvtsi32_si128(crc);
	temp = _mm512_castsi128_si512(b);

	if (data_len > 255) {
		fold0 = _mm512_loadu_si512((const __m512i *)data);
		fold1 = _mm512_loadu_si512((const __m512i *)(data+64));
		fold2 = _mm512_loadu_si512((const __m512i *)(data+128));
		fold3 = _mm512_loadu_si512((const __m512i *)(data+192));
		fold0 = _mm512_xor_si512(fold0, temp);

		/* Main folding loop */
		k = params->rk1_rk2;
		for (n = 256; (n + 256) <= data_len; n += 256) {
			qw0 = _mm512_loadu_si512((const __m512i *)&data[n]);
			qw1 = _mm512_loadu_si512((const __m512i *)
					&(data[n+64]));
			qw2 = _mm512_loadu_si512((const __m512i *)
					&(data[n+128]));
			qw3 = _mm512_loadu_si512((const __m512i *)
					&(data[n+192]));
			fold0 = crcr32_folding_round(qw0, k, fold0);
			fold1 = crcr32_folding_round(qw1, k, fold1);
			fold2 = crcr32_folding_round(qw2, k, fold2);
			fold3 = crcr32_folding_round(qw3, k, fold3);
		}

		/* 256 to 128 fold */
		k = params->rk3_rk4;
		fold0 = crcr32_folding_round(fold2, k, fold0);
		fold1 = crcr32_folding_round(fold3, k, fold1);

		res = crc32_fold_128(fold0, fold1, params);

		reduction = 240 - ((n+256)-data_len);

		while (reduction > 0)
			reduction_loop(&res, &reduction, data, &n,
					params);

		reduction += 16;

		if (n != data_len)
			res = last_two_xmm(data, data_len, n, res,
					params);
	} else {
		if (data_len > 31) {
			res = _mm_cvtsi32_si128(crc);
			d = _mm_loadu_si128((const __m128i *)data);
			res = _mm_xor_si128(res, d);
			n += 16;

			reduction = 240 - ((n+256)-data_len);

			while (reduction > 0)
				reduction_loop(&res, &reduction, data, &n,
						params);

			if (n != data_len)
				res = last_two_xmm(data, data_len, n, res,
						params);
		} else if (data_len > 16) {
			res = _mm_cvtsi32_si128(crc);
			d = _mm_loadu_si128((const __m128i *)data);
			res = _mm_xor_si128(res, d);
			n += 16;

			if (n != data_len)
				res = last_two_xmm(data, data_len, n, res,
						params);
		} else if (data_len == 16) {
			res = _mm_cvtsi32_si128(crc);
			d = _mm_loadu_si128((const __m128i *)data);
			res = _mm_xor_si128(res, d);
		} else {
			res = _mm_cvtsi32_si128(crc);
			mask = byte_len_to_mask_table[data_len];
			d = _mm_maskz_loadu_epi8(mask, data);
			res = _mm_xor_si128(res, d);

			if (data_len > 3) {
				d = _mm_loadu_si128((const __m128i *)
						&shf_table[data_len]);
				res = _mm_shuffle_epi8(res, d);
			} else if (data_len > 2) {
				res = _mm_slli_si128(res, 5);
				goto do_barrett_reduction;
			} else if (data_len > 1) {
				res = _mm_slli_si128(res, 6);
				goto do_barrett_reduction;
			} else if (data_len > 0) {
				res = _mm_slli_si128(res, 7);
				goto do_barrett_reduction;
			} else {
				/* zero length case */
				return crc;
			}
		}
	}

	res = done_128(res, params);

do_barrett_reduction:
	n = barrett_reduction(res, params);

	return n;
}

static void
crc32_load_init_constants(void)
{
	__m128i a;
	/* fold constants */
	uint64_t c0 = 0x00000000e95c1271;
	uint64_t c1 = 0x00000000ce3371cb;
	uint64_t c2 = 0x00000000910eeec1;
	uint64_t c3 = 0x0000000033fff533;
	uint64_t c4 = 0x000000000cbec0ed;
	uint64_t c5 = 0x0000000031f8303f;
	uint64_t c6 = 0x0000000057c54819;
	uint64_t c7 = 0x00000000df068dc2;
	uint64_t c8 = 0x00000000ae0b5394;
	uint64_t c9 = 0x000000001c279815;
	uint64_t c10 = 0x000000001d9513d7;
	uint64_t c11 = 0x000000008f352d95;
	uint64_t c12 = 0x00000000af449247;
	uint64_t c13 = 0x000000003db1ecdc;
	uint64_t c14 = 0x0000000081256527;
	uint64_t c15 = 0x00000000f1da05aa;
	uint64_t c16 = 0x00000000ccaa009e;
	uint64_t c17 = 0x00000000ae689191;
	uint64_t c18 = 0x00000000ccaa009e;
	uint64_t c19 = 0x00000000b8bc6765;
	uint64_t c20 = 0x00000001f7011640;
	uint64_t c21 = 0x00000001db710640;

	a = _mm_set_epi64x(c1, c0);
	crc32_eth.rk1_rk2 = _mm512_broadcast_i32x4(a);

	a = _mm_set_epi64x(c3, c2);
	crc32_eth.rk3_rk4 = _mm512_broadcast_i32x4(a);

	crc32_eth.fold_7x128b = _mm512_setr_epi64(c4, c5, c6, c7, c8,
			c9, c10, c11);
	crc32_eth.fold_3x128b = _mm512_setr_epi64(c12, c13, c14, c15,
			c16, c17, 0, 0);
	crc32_eth.fold_1x128b = _mm_setr_epi64(_mm_cvtsi64_m64(c16),
			_mm_cvtsi64_m64(c17));

	crc32_eth.rk5_rk6 = _mm_setr_epi64(_mm_cvtsi64_m64(c18),
			_mm_cvtsi64_m64(c19));
	crc32_eth.rk7_rk8 = _mm_setr_epi64(_mm_cvtsi64_m64(c20),
			_mm_cvtsi64_m64(c21));
}

static void
crc16_load_init_constants(void)
{
	__m128i a;
	/* fold constants */
	uint64_t c0 = 0x0000000000009a19;
	uint64_t c1 = 0x0000000000002df8;
	uint64_t c2 = 0x00000000000068af;
	uint64_t c3 = 0x000000000000b6c9;
	uint64_t c4 = 0x000000000000c64f;
	uint64_t c5 = 0x000000000000cd95;
	uint64_t c6 = 0x000000000000d341;
	uint64_t c7 = 0x000000000000b8f2;
	uint64_t c8 = 0x0000000000000842;
	uint64_t c9 = 0x000000000000b072;
	uint64_t c10 = 0x00000000000047e3;
	uint64_t c11 = 0x000000000000922d;
	uint64_t c12 = 0x0000000000000e3a;
	uint64_t c13 = 0x0000000000004d7a;
	uint64_t c14 = 0x0000000000005b44;
	uint64_t c15 = 0x0000000000007762;
	uint64_t c16 = 0x00000000000081bf;
	uint64_t c17 = 0x0000000000008e10;
	uint64_t c18 = 0x00000000000081bf;
	uint64_t c19 = 0x0000000000001cbb;
	uint64_t c20 = 0x000000011c581910;
	uint64_t c21 = 0x0000000000010810;

	a = _mm_set_epi64x(c1, c0);
	crc16_ccitt.rk1_rk2 = _mm512_broadcast_i32x4(a);

	a = _mm_set_epi64x(c3, c2);
	crc16_ccitt.rk3_rk4 = _mm512_broadcast_i32x4(a);

	crc16_ccitt.fold_7x128b = _mm512_setr_epi64(c4, c5, c6, c7, c8,
			c9, c10, c11);
	crc16_ccitt.fold_3x128b = _mm512_setr_epi64(c12, c13, c14, c15,
			c16, c17, 0, 0);
	crc16_ccitt.fold_1x128b = _mm_setr_epi64(_mm_cvtsi64_m64(c16),
			_mm_cvtsi64_m64(c17));

	crc16_ccitt.rk5_rk6 = _mm_setr_epi64(_mm_cvtsi64_m64(c18),
			_mm_cvtsi64_m64(c19));
	crc16_ccitt.rk7_rk8 = _mm_setr_epi64(_mm_cvtsi64_m64(c20),
			_mm_cvtsi64_m64(c21));
}

void
rte_net_crc_avx512_init(void)
{
	crc32_load_init_constants();
	crc16_load_init_constants();

	/*
	 * Reset the register as following calculation may
	 * use other data types such as float, double, etc.
	 */
	_mm_empty();
}

uint32_t
rte_crc16_ccitt_avx512_handler(const uint8_t *data, uint32_t data_len)
{
	/* return 16-bit CRC value */
	return (uint16_t)~crc32_eth_calc_vpclmulqdq(data,
		data_len,
		0xffff,
		&crc16_ccitt);
}

uint32_t
rte_crc32_eth_avx512_handler(const uint8_t *data, uint32_t data_len)
{
	/* return 32-bit CRC value */
	return ~crc32_eth_calc_vpclmulqdq(data,
		data_len,
		0xffffffffUL,
		&crc32_eth);
}
