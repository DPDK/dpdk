/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Cavium, Inc. All rights reserved.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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

#ifndef _RTE_CRC_ARM64_H_
#define _RTE_CRC_ARM64_H_

/**
 * @file
 *
 * RTE CRC arm64 Hash
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_cpuflags.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>

static inline uint32_t
crc32c_arm64_u8(uint8_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32cb %w[crc], %w[crc], %w[value]"
			: [crc] "+r" (init_val)
			: [value] "r" (data));
	return init_val;
}

static inline uint32_t
crc32c_arm64_u16(uint16_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32ch %w[crc], %w[crc], %w[value]"
			: [crc] "+r" (init_val)
			: [value] "r" (data));
	return init_val;
}

static inline uint32_t
crc32c_arm64_u32(uint32_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32cw %w[crc], %w[crc], %w[value]"
			: [crc] "+r" (init_val)
			: [value] "r" (data));
	return init_val;
}

static inline uint32_t
crc32c_arm64_u64(uint64_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32cx %w[crc], %w[crc], %x[value]"
			: [crc] "+r" (init_val)
			: [value] "r" (data));
	return init_val;
}

/**
 * Allow or disallow use of arm64 SIMD instrinsics for CRC32 hash
 * calculation.
 *
 * @param alg
 *   An OR of following flags:
 *   - (CRC32_SW) Don't use arm64 crc intrinsics
 *   - (CRC32_ARM64) Use ARMv8 CRC intrinsic if available
 *
 */
static inline void
rte_hash_crc_set_alg(uint8_t alg)
{
	switch (alg) {
	case CRC32_ARM64:
		if (!rte_cpu_get_flag_enabled(RTE_CPUFLAG_CRC32))
			alg = CRC32_SW;
		/* fall-through */
	case CRC32_SW:
		crc32_alg = alg;
		/* fall-through */
	default:
		break;
	}
}

/* Setting the best available algorithm */
static inline void __attribute__((constructor))
rte_hash_crc_init_alg(void)
{
	rte_hash_crc_set_alg(CRC32_ARM64);
}

/**
 * Use single crc32 instruction to perform a hash on a 1 byte value.
 * Fall back to software crc32 implementation in case arm64 crc intrinsics is
 * not supported
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_1byte(uint8_t data, uint32_t init_val)
{
	if (likely(crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u8(data, init_val);

	return crc32c_1byte(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 2 bytes value.
 * Fall back to software crc32 implementation in case arm64 crc intrinsics is
 * not supported
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_2byte(uint16_t data, uint32_t init_val)
{
	if (likely(crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u16(data, init_val);

	return crc32c_2bytes(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 4 byte value.
 * Fall back to software crc32 implementation in case arm64 crc intrinsics is
 * not supported
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_4byte(uint32_t data, uint32_t init_val)
{
	if (likely(crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u32(data, init_val);

	return crc32c_1word(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 8 byte value.
 * Fall back to software crc32 implementation in case arm64 crc intrinsics is
 * not supported
 *
 * @param data
 *   Data to perform hash on.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc_8byte(uint64_t data, uint32_t init_val)
{
	if (likely(crc32_alg == CRC32_ARM64))
		return crc32c_arm64_u64(data, init_val);

	return crc32c_2words(data, init_val);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_CRC_ARM64_H_ */
