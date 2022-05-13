/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_HASH_CRC_H_
#define _RTE_HASH_CRC_H_

/**
 * @file
 *
 * RTE CRC Hash
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_config.h>
#include <rte_cpuflags.h>
#include <rte_branch_prediction.h>
#include <rte_common.h>

#include "rte_crc_sw.h"

#define CRC32_SW            (1U << 0)
#define CRC32_SSE42         (1U << 1)
#define CRC32_x64           (1U << 2)
#define CRC32_SSE42_x64     (CRC32_x64|CRC32_SSE42)
#define CRC32_ARM64         (1U << 3)

static uint8_t crc32_alg = CRC32_SW;

#if defined(RTE_ARCH_ARM64) && defined(__ARM_FEATURE_CRC32)
#include "rte_crc_arm64.h"
#else
#include "rte_crc_x86.h"

/**
 * Allow or disallow use of SSE4.2 instrinsics for CRC32 hash
 * calculation.
 *
 * @param alg
 *   An OR of following flags:
 *   - (CRC32_SW) Don't use SSE4.2 intrinsics
 *   - (CRC32_SSE42) Use SSE4.2 intrinsics if available
 *   - (CRC32_SSE42_x64) Use 64-bit SSE4.2 intrinsic if available (default)
 *
 */
static inline void
rte_hash_crc_set_alg(uint8_t alg)
{
#if defined(RTE_ARCH_X86)
	if (alg == CRC32_SSE42_x64 &&
			!rte_cpu_get_flag_enabled(RTE_CPUFLAG_EM64T))
		alg = CRC32_SSE42;
#endif
	crc32_alg = alg;
}

/* Setting the best available algorithm */
RTE_INIT(rte_hash_crc_init_alg)
{
	rte_hash_crc_set_alg(CRC32_SSE42_x64);
}

/**
 * Use single crc32 instruction to perform a hash on a byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
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
#if defined RTE_ARCH_X86
	if (likely(crc32_alg & CRC32_SSE42))
		return crc32c_sse42_u8(data, init_val);
#endif

	return crc32c_1byte(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 2 bytes value.
 * Fall back to software crc32 implementation in case SSE4.2 is
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
#if defined RTE_ARCH_X86
	if (likely(crc32_alg & CRC32_SSE42))
		return crc32c_sse42_u16(data, init_val);
#endif

	return crc32c_2bytes(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 4 byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
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
#if defined RTE_ARCH_X86
	if (likely(crc32_alg & CRC32_SSE42))
		return crc32c_sse42_u32(data, init_val);
#endif

	return crc32c_1word(data, init_val);
}

/**
 * Use single crc32 instruction to perform a hash on a 8 byte value.
 * Fall back to software crc32 implementation in case SSE4.2 is
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
#ifdef RTE_ARCH_X86_64
	if (likely(crc32_alg == CRC32_SSE42_x64))
		return crc32c_sse42_u64(data, init_val);
#endif

#if defined RTE_ARCH_X86
	if (likely(crc32_alg & CRC32_SSE42))
		return crc32c_sse42_u64_mimic(data, init_val);
#endif

	return crc32c_2words(data, init_val);
}

#endif

/**
 * Calculate CRC32 hash on user-supplied byte array.
 *
 * @param data
 *   Data to perform hash on.
 * @param data_len
 *   How many bytes to use to calculate hash value.
 * @param init_val
 *   Value to initialise hash generator.
 * @return
 *   32bit calculated hash value.
 */
static inline uint32_t
rte_hash_crc(const void *data, uint32_t data_len, uint32_t init_val)
{
	unsigned i;
	uintptr_t pd = (uintptr_t) data;

	for (i = 0; i < data_len / 8; i++) {
		init_val = rte_hash_crc_8byte(*(const uint64_t *)pd, init_val);
		pd += 8;
	}

	if (data_len & 0x4) {
		init_val = rte_hash_crc_4byte(*(const uint32_t *)pd, init_val);
		pd += 4;
	}

	if (data_len & 0x2) {
		init_val = rte_hash_crc_2byte(*(const uint16_t *)pd, init_val);
		pd += 2;
	}

	if (data_len & 0x1)
		init_val = rte_hash_crc_1byte(*(const uint8_t *)pd, init_val);

	return init_val;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_HASH_CRC_H_ */
