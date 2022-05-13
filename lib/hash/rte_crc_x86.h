/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_CRC_X86_H_
#define _RTE_CRC_X86_H_

static inline uint32_t
crc32c_sse42_u8(uint8_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32b %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u16(uint16_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32w %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u32(uint32_t data, uint32_t init_val)
{
	__asm__ volatile(
			"crc32l %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return init_val;
}

static inline uint32_t
crc32c_sse42_u64_mimic(uint64_t data, uint64_t init_val)
{
	union {
		uint32_t u32[2];
		uint64_t u64;
	} d;

	d.u64 = data;
	init_val = crc32c_sse42_u32(d.u32[0], (uint32_t)init_val);
	init_val = crc32c_sse42_u32(d.u32[1], (uint32_t)init_val);
	return (uint32_t)init_val;
}

static inline uint32_t
crc32c_sse42_u64(uint64_t data, uint64_t init_val)
{
	__asm__ volatile(
			"crc32q %[data], %[init_val];"
			: [init_val] "+r" (init_val)
			: [data] "rm" (data));
	return (uint32_t)init_val;
}

#endif /* _RTE_CRC_X86_H_ */
