/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdalign.h>
#include <string.h>

#include <rte_common.h>
#include <rte_branch_prediction.h>
#include <rte_net_crc.h>
#include <rte_vect.h>
#include <rte_cpuflags.h>

#include "net_crc.h"

/** PMULL CRC computation context structure */
struct crc_pmull_ctx {
	uint64x2_t rk1_rk2;
	uint64x2_t rk3_rk4;
	uint64x2_t rk5_rk6;
	uint64x2_t rk7_rk8;
};

alignas(16) struct crc_pmull_ctx crc32_eth_pmull;
alignas(16) struct crc_pmull_ctx crc16_ccitt_pmull;

static const alignas(16) uint8_t crc_neon_shift_tab[32] = {
	0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8,
	0xf7, 0xf6, 0xf5, 0xf4, 0xf3, 0xf2, 0xf1, 0xf0,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
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
static inline uint64x2_t
neon_shift_left(uint64x2_t reg, const unsigned int num)
{
	uint8x16_t tbl = vld1q_u8(crc_neon_shift_tab + 16 - num);
	return vreinterpretq_u64_u8(vqtbl1q_u8(vreinterpretq_u8_u64(reg), tbl));
}

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
			vgetq_lane_p64(vreinterpretq_p64_u64(fold), 0),
			vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 0)));

	uint64x2_t tmp1 = vreinterpretq_u64_p128(vmull_high_p64(
			vreinterpretq_p64_u64(fold),
			vreinterpretq_p64_u64(precomp)));

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
	uint64x2_t tmp0, tmp1, tmp2;

	tmp0 = vreinterpretq_u64_u32(
		vsetq_lane_u32(0, vreinterpretq_u32_u64(data64), 0));

	tmp1 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(tmp0), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 0)));
	tmp1 = veorq_u64(tmp1, tmp0);

	tmp2 = vreinterpretq_u64_p128(vmull_p64(
		vgetq_lane_p64(vreinterpretq_p64_u64(tmp1), 0),
		vgetq_lane_p64(vreinterpretq_p64_u64(precomp), 1)));
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
	 * Folding all data into 4 parallel 16 byte data block
	 * Later folds 4 parallel blocks into single fold block
	 */
	if (likely(data_len >= 64)) {
		uint64x2_t fold1, fold2, fold3, fold4;
		uint64x2_t temp1, temp2, temp3, temp4;
		fold1 = vld1q_u64((const uint64_t *)(data +  0));
		fold2 = vld1q_u64((const uint64_t *)(data + 16));
		fold3 = vld1q_u64((const uint64_t *)(data + 32));
		fold4 = vld1q_u64((const uint64_t *)(data + 48));
		fold1 = veorq_u64(fold1, temp);
		k = params->rk1_rk2;

		for (n = 64; (n + 64) <= data_len; n += 64) {
			temp1 = vld1q_u64((const uint64_t *)&data[n +  0]);
			temp2 = vld1q_u64((const uint64_t *)&data[n + 16]);
			temp3 = vld1q_u64((const uint64_t *)&data[n + 32]);
			temp4 = vld1q_u64((const uint64_t *)&data[n + 48]);
			fold1 = crcr32_folding_round(temp1, k, fold1);
			fold2 = crcr32_folding_round(temp2, k, fold2);
			fold3 = crcr32_folding_round(temp3, k, fold3);
			fold4 = crcr32_folding_round(temp4, k, fold4);
		}
		k = params->rk3_rk4;
		fold1 = crcr32_folding_round(fold2, k, fold1);
		fold1 = crcr32_folding_round(fold3, k, fold1);
		fold = crcr32_folding_round(fold4, k, fold1);
		goto single_fold_loop;
	}

	if (unlikely(data_len < 16)) {
		/* 0 to 15 bytes */
		alignas(16) uint8_t buffer[16];

		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, data, data_len);

		fold = vld1q_u64((uint64_t *)buffer);
		fold = veorq_u64(fold, temp);
		if (unlikely(data_len < 4)) {
			fold = neon_shift_left(fold, 8 - data_len);
			goto barret_reduction;
		}
		fold = neon_shift_left(fold, 16 - data_len);
		goto reduction_128_64;
	}

	/** At least 16 bytes in the buffer */
	/** Apply CRC initial value */
	fold = vld1q_u64((const uint64_t *)data);
	fold = veorq_u64(fold, temp);

	/** Single folding loop - the last 16 bytes is processed separately */
	k = params->rk3_rk4;
	n = 16;

single_fold_loop:
	for (; (n + 16) <= data_len; n += 16) {
		temp = vld1q_u64((const uint64_t *)&data[n]);
		fold = crcr32_folding_round(temp, k, fold);
	}

	/** Partial bytes - process last <16 bytes */
	if (likely(n < data_len)) {
		uint8x16_t last16, t1, t2;
		uint64x2_t a, b;
		uint32_t rem = data_len & 15;

		last16 = vld1q_u8((const uint8_t *)&data[data_len - 16]);
		t1 = vld1q_u8(crc_neon_shift_tab + rem);
		a  = vreinterpretq_u64_u8(vqtbl1q_u8(vreinterpretq_u8_u64(fold), t1));
		t2 = vmvnq_u8(t1);
		t2 = vqtbl1q_u8(vreinterpretq_u8_u64(fold), t2);
		t1 = vcgezq_s8(vreinterpretq_s8_u8(t1));
		b  = vreinterpretq_u64_u8(vbslq_u8(t1, last16, t2));

		/* k = rk3 & rk4 */
		fold = crcr32_folding_round(b, k, a);
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

void
rte_net_crc_neon_init(void)
{
	/* Initialize CRC16 data */
	uint64_t ccitt_k1_k2[2] = {0x19a3cLLU, 0x14ff2LLU};
	uint64_t ccitt_k3_k4[2] = {0x8e10LLU, 0x189aeLLU};
	uint64_t ccitt_k5_k6[2] = {0x189aeLLU, 0x114aaLLU};
	uint64_t ccitt_k7_k8[2] = {0x11c581910LLU, 0x10811LLU};

	/* Initialize CRC32 data */
	uint64_t eth_k1_k2[2] = {0x154442bd4LLU, 0x1c6e41596LLU};
	uint64_t eth_k3_k4[2] = {0x1751997d0LLU, 0xccaa009eLLU};
	uint64_t eth_k5_k6[2] = {0xccaa009eLLU, 0x163cd6124LLU};
	uint64_t eth_k7_k8[2] = {0x1f7011640LLU, 0x1db710641LLU};

	/** Save the params in context structure */
	crc16_ccitt_pmull.rk1_rk2 = vld1q_u64(ccitt_k1_k2);
	crc16_ccitt_pmull.rk3_rk4 = vld1q_u64(ccitt_k3_k4);
	crc16_ccitt_pmull.rk5_rk6 = vld1q_u64(ccitt_k5_k6);
	crc16_ccitt_pmull.rk7_rk8 = vld1q_u64(ccitt_k7_k8);

	/** Save the params in context structure */
	crc32_eth_pmull.rk1_rk2 = vld1q_u64(eth_k1_k2);
	crc32_eth_pmull.rk3_rk4 = vld1q_u64(eth_k3_k4);
	crc32_eth_pmull.rk5_rk6 = vld1q_u64(eth_k5_k6);
	crc32_eth_pmull.rk7_rk8 = vld1q_u64(eth_k7_k8);
}

uint32_t
rte_crc16_ccitt_neon_handler(const uint8_t *data, uint32_t data_len)
{
	return (uint16_t)~crc32_eth_calc_pmull(data,
		data_len,
		0xffff,
		&crc16_ccitt_pmull);
}

uint32_t
rte_crc32_eth_neon_handler(const uint8_t *data, uint32_t data_len)
{
	return ~crc32_eth_calc_pmull(data,
		data_len,
		0xffffffffUL,
		&crc32_eth_pmull);
}
