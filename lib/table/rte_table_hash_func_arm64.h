/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Linaro Limited
 */

#ifndef __INCLUDE_RTE_TABLE_HASH_FUNC_ARM64_H__
#define __INCLUDE_RTE_TABLE_HASH_FUNC_ARM64_H__

#define _CRC32CX(crc, val)	\
	__asm__("crc32cx %w[c], %w[c], %x[v]":[c] "+r" (crc):[v] "r" (val))

static inline uint64_t
rte_crc32_u64(uint64_t crc, uint64_t v)
{
	uint32_t crc32 = crc;

	_CRC32CX(crc32, v);

	return crc32;
}

#endif
