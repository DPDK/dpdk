/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_NET_CRC_SSE_H_
#define _RTE_NET_CRC_SSE_H_

#include <rte_branch_prediction.h>

#include <x86intrin.h>
#include <cpuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/** PCLMULQDQ CRC computation context structure */
struct crc_pclmulqdq_ctx {
	__m128i rk1_rk2;
	__m128i rk5_rk6;
	__m128i rk7_rk8;
};

static struct crc_pclmulqdq_ctx crc32_eth_pclmulqdq __rte_aligned(16);
static struct crc_pclmulqdq_ctx crc16_ccitt_pclmulqdq __rte_aligned(16);
/**
 * @brief Performs one folding round
 *
 * Logically function operates as follows:
 *     DATA = READ_NEXT_16BYTES();
 *     F1 = LSB8(FOLD)
 *     F2 = MSB8(FOLD)
 *     T1 = CLMUL(F1, RK1)
 *     T2 = CLMUL(F2, RK2)
 *     FOLD = XOR(T1, T2, DATA)
 *
 * @param data_block
 *   16 byte data block
 * @param precomp
 *   Precomputed rk1 constant
 * @param fold
 *   Current16 byte folded data
 *
 * @return
 *   New 16 byte folded data
 */
static __rte_always_inline __m128i
crcr32_folding_round(__m128i data_block,
		__m128i precomp,
		__m128i fold)
{
	__m128i tmp0 = _mm_clmulepi64_si128(fold, precomp, 0x01);
	__m128i tmp1 = _mm_clmulepi64_si128(fold, precomp, 0x10);

	return _mm_xor_si128(tmp1, _mm_xor_si128(data_block, tmp0));
}

/**
 * Performs reduction from 128 bits to 64 bits
 *
 * @param data128
 *   128 bits data to be reduced
 * @param precomp
 *   precomputed constants rk5, rk6
 *
 * @return
 *  64 bits reduced data
 */

static __rte_always_inline __m128i
crcr32_reduce_128_to_64(__m128i data128, __m128i precomp)
{
	__m128i tmp0, tmp1, tmp2;

	/* 64b fold */
	tmp0 = _mm_clmulepi64_si128(data128, precomp, 0x00);
	tmp1 = _mm_srli_si128(data128, 8);
	tmp0 = _mm_xor_si128(tmp0, tmp1);

	/* 32b fold */
	tmp2 = _mm_slli_si128(tmp0, 4);
	tmp1 = _mm_clmulepi64_si128(tmp2, precomp, 0x10);

	return _mm_xor_si128(tmp1, tmp0);
}

/**
 * Performs Barret's reduction from 64 bits to 32 bits
 *
 * @param data64
 *   64 bits data to be reduced
 * @param precomp
 *   rk7 precomputed constant
 *
 * @return
 *   reduced 32 bits data
 */

static __rte_always_inline uint32_t
crcr32_reduce_64_to_32(__m128i data64, __m128i precomp)
{
	static const uint32_t mask1[4] __rte_aligned(16) = {
		0xffffffff, 0xffffffff, 0x00000000, 0x00000000
	};

	static const uint32_t mask2[4] __rte_aligned(16) = {
		0x00000000, 0xffffffff, 0xffffffff, 0xffffffff
	};
	__m128i tmp0, tmp1, tmp2;

	tmp0 = _mm_and_si128(data64, _mm_load_si128((const __m128i *)mask2));

	tmp1 = _mm_clmulepi64_si128(tmp0, precomp, 0x00);
	tmp1 = _mm_xor_si128(tmp1, tmp0);
	tmp1 = _mm_and_si128(tmp1, _mm_load_si128((const __m128i *)mask1));

	tmp2 = _mm_clmulepi64_si128(tmp1, precomp, 0x10);
	tmp2 = _mm_xor_si128(tmp2, tmp1);
	tmp2 = _mm_xor_si128(tmp2, tmp0);

	return _mm_extract_epi32(tmp2, 2);
}

static const uint8_t crc_xmm_shift_tab[48] __rte_aligned(16) = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

/**
 * Shifts left 128 bit register by specified number of bytes
 *
 * @param reg
 *   128 bit value
 * @param num
 *   number of bytes to shift left reg by (0-16)
 *
 * @return
 *   reg << (num * 8)
 */

static __rte_always_inline __m128i
xmm_shift_left(__m128i reg, const unsigned int num)
{
	const __m128i *p = (const __m128i *)(crc_xmm_shift_tab + 16 - num);

	return _mm_shuffle_epi8(reg, _mm_loadu_si128(p));
}

static __rte_always_inline uint32_t
crc32_eth_calc_pclmulqdq(
	const uint8_t *data,
	uint32_t data_len,
	uint32_t crc,
	const struct crc_pclmulqdq_ctx *params)
{
	__m128i temp, fold, k;
	uint32_t n;

	/* Get CRC init value */
	temp = _mm_insert_epi32(_mm_setzero_si128(), crc, 0);

	/**
	 * Folding all data into single 16 byte data block
	 * Assumes: fold holds first 16 bytes of data
	 */

	if (unlikely(data_len < 32)) {
		if (unlikely(data_len == 16)) {
			/* 16 bytes */
			fold = _mm_loadu_si128((const __m128i *)data);
			fold = _mm_xor_si128(fold, temp);
			goto reduction_128_64;
		}

		if (unlikely(data_len < 16)) {
			/* 0 to 15 bytes */
			uint8_t buffer[16] __rte_aligned(16);

			memset(buffer, 0, sizeof(buffer));
			memcpy(buffer, data, data_len);

			fold = _mm_load_si128((const __m128i *)buffer);
			fold = _mm_xor_si128(fold, temp);
			if (unlikely(data_len < 4)) {
				fold = xmm_shift_left(fold, 8 - data_len);
				goto barret_reduction;
			}
			fold = xmm_shift_left(fold, 16 - data_len);
			goto reduction_128_64;
		}
		/* 17 to 31 bytes */
		fold = _mm_loadu_si128((const __m128i *)data);
		fold = _mm_xor_si128(fold, temp);
		n = 16;
		k = params->rk1_rk2;
		goto partial_bytes;
	}

	/** At least 32 bytes in the buffer */
	/** Apply CRC initial value */
	fold = _mm_loadu_si128((const __m128i *)data);
	fold = _mm_xor_si128(fold, temp);

	/** Main folding loop - the last 16 bytes is processed separately */
	k = params->rk1_rk2;
	for (n = 16; (n + 16) <= data_len; n += 16) {
		temp = _mm_loadu_si128((const __m128i *)&data[n]);
		fold = crcr32_folding_round(temp, k, fold);
	}

partial_bytes:
	if (likely(n < data_len)) {

		const uint32_t mask3[4] __rte_aligned(16) = {
			0x80808080, 0x80808080, 0x80808080, 0x80808080
		};

		const uint8_t shf_table[32] __rte_aligned(16) = {
			0x00, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
			0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
			0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
		};

		__m128i last16, a, b;

		last16 = _mm_loadu_si128((const __m128i *)&data[data_len - 16]);

		temp = _mm_loadu_si128((const __m128i *)
			&shf_table[data_len & 15]);
		a = _mm_shuffle_epi8(fold, temp);

		temp = _mm_xor_si128(temp,
			_mm_load_si128((const __m128i *)mask3));
		b = _mm_shuffle_epi8(fold, temp);
		b = _mm_blendv_epi8(b, last16, temp);

		/* k = rk1 & rk2 */
		temp = _mm_clmulepi64_si128(a, k, 0x01);
		fold = _mm_clmulepi64_si128(a, k, 0x10);

		fold = _mm_xor_si128(fold, temp);
		fold = _mm_xor_si128(fold, b);
	}

	/** Reduction 128 -> 32 Assumes: fold holds 128bit folded data */
reduction_128_64:
	k = params->rk5_rk6;
	fold = crcr32_reduce_128_to_64(fold, k);

barret_reduction:
	k = params->rk7_rk8;
	n = crcr32_reduce_64_to_32(fold, k);

	return n;
}


static inline void
rte_net_crc_sse42_init(void)
{
	uint64_t k1, k2, k5, k6;
	uint64_t p = 0, q = 0;

	/** Initialize CRC16 data */
	k1 = 0x189aeLLU;
	k2 = 0x8e10LLU;
	k5 = 0x189aeLLU;
	k6 = 0x114aaLLU;
	q =  0x11c581910LLU;
	p =  0x10811LLU;

	/** Save the params in context structure */
	crc16_ccitt_pclmulqdq.rk1_rk2 =
		_mm_setr_epi64(_mm_cvtsi64_m64(k1), _mm_cvtsi64_m64(k2));
	crc16_ccitt_pclmulqdq.rk5_rk6 =
		_mm_setr_epi64(_mm_cvtsi64_m64(k5), _mm_cvtsi64_m64(k6));
	crc16_ccitt_pclmulqdq.rk7_rk8 =
		_mm_setr_epi64(_mm_cvtsi64_m64(q), _mm_cvtsi64_m64(p));

	/** Initialize CRC32 data */
	k1 = 0xccaa009eLLU;
	k2 = 0x1751997d0LLU;
	k5 = 0xccaa009eLLU;
	k6 = 0x163cd6124LLU;
	q =  0x1f7011640LLU;
	p =  0x1db710641LLU;

	/** Save the params in context structure */
	crc32_eth_pclmulqdq.rk1_rk2 =
		_mm_setr_epi64(_mm_cvtsi64_m64(k1), _mm_cvtsi64_m64(k2));
	crc32_eth_pclmulqdq.rk5_rk6 =
		_mm_setr_epi64(_mm_cvtsi64_m64(k5), _mm_cvtsi64_m64(k6));
	crc32_eth_pclmulqdq.rk7_rk8 =
		_mm_setr_epi64(_mm_cvtsi64_m64(q), _mm_cvtsi64_m64(p));

	/**
	 * Reset the register as following calculation may
	 * use other data types such as float, double, etc.
	 */
	_mm_empty();

}

static inline uint32_t
rte_crc16_ccitt_sse42_handler(const uint8_t *data,
	uint32_t data_len)
{
	/** return 16-bit CRC value */
	return (uint16_t)~crc32_eth_calc_pclmulqdq(data,
		data_len,
		0xffff,
		&crc16_ccitt_pclmulqdq);
}

static inline uint32_t
rte_crc32_eth_sse42_handler(const uint8_t *data,
	uint32_t data_len)
{
	return ~crc32_eth_calc_pclmulqdq(data,
		data_len,
		0xffffffffUL,
		&crc32_eth_pclmulqdq);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_NET_CRC_SSE_H_ */
