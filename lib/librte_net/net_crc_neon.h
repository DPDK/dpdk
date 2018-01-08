/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _NET_CRC_NEON_H_
#define _NET_CRC_NEON_H_

#include <rte_branch_prediction.h>
#include <rte_net_crc.h>
#include <rte_vect.h>
#include <rte_cpuflags.h>

#ifdef __cplusplus
extern "C" {
#endif

/** PMULL CRC computation context structure */
struct crc_pmull_ctx {
	uint64x2_t rk1_rk2;
	uint64x2_t rk5_rk6;
	uint64x2_t rk7_rk8;
};

struct crc_pmull_ctx crc32_eth_pmull __rte_aligned(16);
struct crc_pmull_ctx crc16_ccitt_pmull __rte_aligned(16);

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
 * @param data_block 16 byte data block
 * @param precomp precomputed rk1 constant
 * @param fold running 16 byte folded data
 *
 * @return New 16 byte folded data
 */
static inline uint64x2_t
crcr32_folding_round(uint64x2_t data_block, uint64x2_t precomp,
	uint64x2_t fold)
{
	uint64x2_t tmp0 = vreinterpretq_u64_p128(vmull_p64(
			vgetq_lane_p64(vreinterpretq_p64_u64(fold), 1),
			vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 0)));

	uint64x2_t tmp1 = vreinterpretq_u64_p128(vmull_p64(
			vgetq_lane_p64(vreinterpretq_p64_u64(fold), 0),
			vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 1)));

	return veorq_u64(tmp1, veorq_u64(data_block, tmp0));
}

/**
 * Performs reduction from 128 bits to 64 bits
 *
 * @param data128 128 bits data to be reduced
 * @param precomp rk5 and rk6 precomputed constants
 *
 * @return data reduced to 64 bits
 */
static inline uint64x2_t
crcr32_reduce_128_to_64(uint64x2_t data128,
	uint64x2_t precomp)
{
	uint64x2_t tmp0, tmp1, tmp2;

	/* 64b fold */
	tmp0 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(data128), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 0)));
	tmp1 = vshift_bytes_right(data128, 8);
	tmp0 = veorq_u64(tmp0, tmp1);

	/* 32b fold */
	tmp2 = vshift_bytes_left(tmp0, 4);
	tmp1 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(tmp2), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 1)));

	return veorq_u64(tmp1, tmp0);
}

/**
 * Performs Barret's reduction from 64 bits to 32 bits
 *
 * @param data64 64 bits data to be reduced
 * @param precomp rk7 precomputed constant
 *
 * @return data reduced to 32 bits
 */
static inline uint32_t
crcr32_reduce_64_to_32(uint64x2_t data64,
	uint64x2_t precomp)
{
	static uint32_t mask1[4] __rte_aligned(16) = {
		0xffffffff, 0xffffffff, 0x00000000, 0x00000000
	};
	static uint32_t mask2[4] __rte_aligned(16) = {
		0x00000000, 0xffffffff, 0xffffffff, 0xffffffff
	};
	uint64x2_t tmp0, tmp1, tmp2;

	tmp0 = vandq_u64(data64, vld1q_u64((uint64_t *)mask2));

	tmp1 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(tmp0), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 0)));
	tmp1 = veorq_u64(tmp1, tmp0);
	tmp1 = vandq_u64(tmp1, vld1q_u64((uint64_t *)mask1));

	tmp2 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(tmp1), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 1)));
	tmp2 = veorq_u64(tmp2, tmp1);
	tmp2 = veorq_u64(tmp2, tmp0);

	return vgetq_lane_u32(vreinterpretq_u32_u64(tmp2), 2);
}

static inline uint32_t
crc32_eth_calc_pmull(
	const uint8_t *data,
	uint32_t data_len,
	uint32_t crc,
	const struct crc_pmull_ctx *params)
{
	uint64x2_t temp, fold, k;
	uint32_t n;

	/* Get CRC init value */
	temp = vreinterpretq_u64_u32(vsetq_lane_u32(crc, vmovq_n_u32(0), 0));

	/**
	 * Folding all data into single 16 byte data block
	 * Assumes: fold holds first 16 bytes of data
	 */
	if (unlikely(data_len < 32)) {
		if (unlikely(data_len == 16)) {
			/* 16 bytes */
			fold = vld1q_u64((const uint64_t *)data);
			fold = veorq_u64(fold, temp);
			goto reduction_128_64;
		}

		if (unlikely(data_len < 16)) {
			/* 0 to 15 bytes */
			uint8_t buffer[16] __rte_aligned(16);

			memset(buffer, 0, sizeof(buffer));
			memcpy(buffer, data, data_len);

			fold = vld1q_u64((uint64_t *)buffer);
			fold = veorq_u64(fold, temp);
			if (unlikely(data_len < 4)) {
				fold = vshift_bytes_left(fold, 8 - data_len);
				goto barret_reduction;
			}
			fold = vshift_bytes_left(fold, 16 - data_len);
			goto reduction_128_64;
		}
		/* 17 to 31 bytes */
		fold = vld1q_u64((const uint64_t *)data);
		fold = veorq_u64(fold, temp);
		n = 16;
		k = params->rk1_rk2;
		goto partial_bytes;
	}

	/** At least 32 bytes in the buffer */
	/** Apply CRC initial value */
	fold = vld1q_u64((const uint64_t *)data);
	fold = veorq_u64(fold, temp);

	/** Main folding loop - the last 16 bytes is processed separately */
	k = params->rk1_rk2;
	for (n = 16; (n + 16) <= data_len; n += 16) {
		temp = vld1q_u64((const uint64_t *)&data[n]);
		fold = crcr32_folding_round(temp, k, fold);
	}

partial_bytes:
	if (likely(n < data_len)) {
		uint64x2_t last16, a, b, mask;
		uint32_t rem = data_len & 15;

		last16 = vld1q_u64((const uint64_t *)&data[data_len - 16]);
		a = vshift_bytes_left(fold, 16 - rem);
		b = vshift_bytes_right(fold, rem);
		mask = vshift_bytes_left(vdupq_n_u64(-1), 16 - rem);
		b = vorrq_u64(b, vandq_u64(mask, last16));

		/* k = rk1 & rk2 */
		temp = vreinterpretq_u64_p128(vmull_p64(
				vgetq_lane_p64(vreinterpretq_p64_u64(a), 1),
				vgetq_lane_p64(vreinterpretq_p64_u64(k), 0)));
		fold = vreinterpretq_u64_p128(vmull_p64(
				vgetq_lane_p64(vreinterpretq_p64_u64(a), 0),
				vgetq_lane_p64(vreinterpretq_p64_u64(k), 1)));
		fold = veorq_u64(fold, temp);
		fold = veorq_u64(fold, b);
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
rte_net_crc_neon_init(void)
{
	/* Initialize CRC16 data */
	uint64_t ccitt_k1_k2[2] = {0x189aeLLU, 0x8e10LLU};
	uint64_t ccitt_k5_k6[2] = {0x189aeLLU, 0x114aaLLU};
	uint64_t ccitt_k7_k8[2] = {0x11c581910LLU, 0x10811LLU};

	/* Initialize CRC32 data */
	uint64_t eth_k1_k2[2] = {0xccaa009eLLU, 0x1751997d0LLU};
	uint64_t eth_k5_k6[2] = {0xccaa009eLLU, 0x163cd6124LLU};
	uint64_t eth_k7_k8[2] = {0x1f7011640LLU, 0x1db710641LLU};

	/** Save the params in context structure */
	crc16_ccitt_pmull.rk1_rk2 = vld1q_u64(ccitt_k1_k2);
	crc16_ccitt_pmull.rk5_rk6 = vld1q_u64(ccitt_k5_k6);
	crc16_ccitt_pmull.rk7_rk8 = vld1q_u64(ccitt_k7_k8);

	/** Save the params in context structure */
	crc32_eth_pmull.rk1_rk2 = vld1q_u64(eth_k1_k2);
	crc32_eth_pmull.rk5_rk6 = vld1q_u64(eth_k5_k6);
	crc32_eth_pmull.rk7_rk8 = vld1q_u64(eth_k7_k8);
}

static inline uint32_t
rte_crc16_ccitt_neon_handler(const uint8_t *data,
	uint32_t data_len)
{
	return (uint16_t)~crc32_eth_calc_pmull(data,
		data_len,
		0xffff,
		&crc16_ccitt_pmull);
}

static inline uint32_t
rte_crc32_eth_neon_handler(const uint8_t *data,
	uint32_t data_len)
{
	return ~crc32_eth_calc_pmull(data,
		data_len,
		0xffffffffUL,
		&crc32_eth_pmull);
}

#ifdef __cplusplus
}
#endif

#endif /* _NET_CRC_NEON_H_ */
