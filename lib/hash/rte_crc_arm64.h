/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

#ifndef _RTE_CRC_ARM64_H_
#define _RTE_CRC_ARM64_H_

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

/*
 * Use single crc32 instruction to perform a hash on a byte value.
 * Fall back to software crc32 implementation in case ARM CRC is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_1byte(uint8_t data, uint32_t init_val)
{
	if (likely(rte_hash_crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u8(data, init_val);

	return crc32c_1byte(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 2 bytes value.
 * Fall back to software crc32 implementation in case ARM CRC is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_2byte(uint16_t data, uint32_t init_val)
{
	if (likely(rte_hash_crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u16(data, init_val);

	return crc32c_2bytes(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 4 byte value.
 * Fall back to software crc32 implementation in case ARM CRC is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_4byte(uint32_t data, uint32_t init_val)
{
	if (likely(rte_hash_crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u32(data, init_val);

	return crc32c_1word(data, init_val);
}

/*
 * Use single crc32 instruction to perform a hash on a 8 byte value.
 * Fall back to software crc32 implementation in case ARM CRC is
 * not supported.
 */
static inline uint32_t
rte_hash_crc_8byte(uint64_t data, uint32_t init_val)
{
	if (likely(rte_hash_crc32_alg & CRC32_ARM64))
		return crc32c_arm64_u64(data, init_val);

	return crc32c_2words(data, init_val);
}

#endif /* _RTE_CRC_ARM64_H_ */
