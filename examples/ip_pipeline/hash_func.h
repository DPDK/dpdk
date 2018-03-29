/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef __INCLUDE_HASH_FUNC_H__
#define __INCLUDE_HASH_FUNC_H__

static inline uint64_t
hash_xor_key8(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0;

	xor0 = seed ^ (k[0] & m[0]);

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key16(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key24(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);

	xor0 ^= k[2] & m[2];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key32(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0, xor1;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);
	xor1 = (k[2] & m[2]) ^ (k[3] & m[3]);

	xor0 ^= xor1;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key40(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0, xor1;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);
	xor1 = (k[2] & m[2]) ^ (k[3] & m[3]);

	xor0 ^= xor1;

	xor0 ^= k[4] & m[4];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key48(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0, xor1, xor2;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);
	xor1 = (k[2] & m[2]) ^ (k[3] & m[3]);
	xor2 = (k[4] & m[4]) ^ (k[5] & m[5]);

	xor0 ^= xor1;

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key56(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0, xor1, xor2;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);
	xor1 = (k[2] & m[2]) ^ (k[3] & m[3]);
	xor2 = (k[4] & m[4]) ^ (k[5] & m[5]);

	xor0 ^= xor1;
	xor2 ^= k[6] & m[6];

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key64(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t xor0, xor1, xor2, xor3;

	xor0 = ((k[0] & m[0]) ^ seed) ^ (k[1] & m[1]);
	xor1 = (k[2] & m[2]) ^ (k[3] & m[3]);
	xor2 = (k[4] & m[4]) ^ (k[5] & m[5]);
	xor3 = (k[6] & m[6]) ^ (k[7] & m[7]);

	xor0 ^= xor1;
	xor2 ^= xor3;

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

#if defined(RTE_ARCH_X86_64)

#include <x86intrin.h>

static inline uint64_t
hash_crc_key8(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t crc0;

	crc0 = _mm_crc32_u64(seed, k[0] & m[0]);

	return crc0;
}

static inline uint64_t
hash_crc_key16(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, crc0, crc1;

	k0 = k[0] & m[0];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key24(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc0 = _mm_crc32_u64(crc0, k2);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key32(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = _mm_crc32_u64(k2, k[3] & m[3]);
	crc3 = k2 >> 32;

	crc0 = _mm_crc32_u64(crc0, crc1);
	crc1 = _mm_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key40(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = _mm_crc32_u64(k2, k[3] & m[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc0 = _mm_crc32_u64(crc0, crc1);
	crc1 = _mm_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key48(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = _mm_crc32_u64(k2, k[3] & m[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc0 = _mm_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = _mm_crc32_u64(crc3, k5);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key56(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = _mm_crc32_u64(k2, k[3] & m[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc4 = _mm_crc32_u64(k5, k[6] & m[6]);
	crc5 = k5 >> 32;

	crc0 = _mm_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = _mm_crc32_u64(crc3, (crc4 << 32) ^ crc5);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key64(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1] & m[1]);

	crc2 = _mm_crc32_u64(k2, k[3] & m[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4] & m[4]);

	crc4 = _mm_crc32_u64(k5, k[6] & m[6]);
	crc5 = _mm_crc32_u64(k5 >> 32, k[7] & m[7]);

	crc0 = _mm_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = _mm_crc32_u64(crc3, (crc4 << 32) ^ crc5);

	crc0 ^= crc1;

	return crc0;
}

#define hash_default_key8			hash_crc_key8
#define hash_default_key16			hash_crc_key16
#define hash_default_key24			hash_crc_key24
#define hash_default_key32			hash_crc_key32
#define hash_default_key40			hash_crc_key40
#define hash_default_key48			hash_crc_key48
#define hash_default_key56			hash_crc_key56
#define hash_default_key64			hash_crc_key64

#elif defined(RTE_ARCH_ARM64)
#include "hash_func_arm64.h"
#else

#define hash_default_key8			hash_xor_key8
#define hash_default_key16			hash_xor_key16
#define hash_default_key24			hash_xor_key24
#define hash_default_key32			hash_xor_key32
#define hash_default_key40			hash_xor_key40
#define hash_default_key48			hash_xor_key48
#define hash_default_key56			hash_xor_key56
#define hash_default_key64			hash_xor_key64

#endif

#endif
