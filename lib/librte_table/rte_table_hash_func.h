/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_HASH_FUNC_H__
#define __INCLUDE_RTE_TABLE_HASH_FUNC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_common.h>

__rte_experimental
static inline uint64_t
rte_crc32_u64_generic(uint64_t crc, uint64_t value)
{
	int i;

	crc = (crc & 0xFFFFFFFFLLU) ^ value;
	for (i = 63; i >= 0; i--) {
		uint64_t mask;

		mask = -(crc & 1LLU);
		crc = (crc >> 1LLU) ^ (0x82F63B78LLU & mask);
	}

	return crc;
}

#if defined(RTE_ARCH_X86_64)

#include <x86intrin.h>

static inline uint64_t
rte_crc32_u64(uint64_t crc, uint64_t v)
{
	return _mm_crc32_u64(crc, v);
}

#elif defined(RTE_ARCH_ARM64) && defined(RTE_MACHINE_CPUFLAG_CRC32)
#include "rte_table_hash_func_arm64.h"
#else

static inline uint64_t
rte_crc32_u64(uint64_t crc, uint64_t v)
{
	return rte_crc32_u64_generic(crc, v);
}

#endif

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key8(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t crc0;

	crc0 = rte_crc32_u64(seed, k[0] & m[0]);

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key16(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, crc0, crc1;

	k0 = k[0] & m[0];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key24(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc0 = rte_crc32_u64(crc0, k2);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key32(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = rte_crc32_u64(k2, k[3] & m[3]);
	crc3 = k2 >> 32;

	crc0 = rte_crc32_u64(crc0, crc1);
	crc1 = rte_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key40(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = rte_crc32_u64(k2, k[3] & m[3]);
	crc3 = rte_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc0 = rte_crc32_u64(crc0, crc1);
	crc1 = rte_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key48(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = rte_crc32_u64(k2, k[3] & m[3]);
	crc3 = rte_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc0 = rte_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = rte_crc32_u64(crc3, k5);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key56(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = rte_crc32_u64(k2, k[3] & m[3]);
	crc3 = rte_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc4 = rte_crc32_u64(k5, k[6] & m[6]);
	crc5 = k5 >> 32;

	crc0 = rte_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = rte_crc32_u64(crc3, (crc4 << 32) ^ crc5);

	crc0 ^= crc1;

	return crc0;
}

__rte_experimental
static inline uint64_t
rte_table_hash_crc_key64(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = rte_crc32_u64(k0, seed);
	crc1 = rte_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = rte_crc32_u64(k2, k[3] & m[3]);
	crc3 = rte_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc4 = rte_crc32_u64(k5, k[6] & m[6]);
	crc5 = rte_crc32_u64(k5 >> 32, k[7] & m[7]);

	crc0 = rte_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = rte_crc32_u64(crc3, (crc4 << 32) ^ crc5);

	crc0 ^= crc1;

	return crc0;
}

#ifdef __cplusplus
}
#endif

#endif
