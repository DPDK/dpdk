/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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
#ifndef __INCLUDE_HASH_FUNC_H__
#define __INCLUDE_HASH_FUNC_H__

static inline uint64_t
hash_xor_key8(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0;

	xor0 = seed ^ k[0];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key16(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0;

	xor0 = (k[0] ^ seed) ^ k[1];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key24(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0;

	xor0 = (k[0] ^ seed) ^ k[1];

	xor0 ^= k[2];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key32(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0, xor1;

	xor0 = (k[0] ^ seed) ^ k[1];
	xor1 = k[2] ^ k[3];

	xor0 ^= xor1;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key40(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0, xor1;

	xor0 = (k[0] ^ seed) ^ k[1];
	xor1 = k[2] ^ k[3];

	xor0 ^= xor1;

	xor0 ^= k[4];

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key48(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0, xor1, xor2;

	xor0 = (k[0] ^ seed) ^ k[1];
	xor1 = k[2] ^ k[3];
	xor2 = k[4] ^ k[5];

	xor0 ^= xor1;

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key56(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0, xor1, xor2;

	xor0 = (k[0] ^ seed) ^ k[1];
	xor1 = k[2] ^ k[3];
	xor2 = k[4] ^ k[5];

	xor0 ^= xor1;
	xor2 ^= k[6];

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

static inline uint64_t
hash_xor_key64(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t xor0, xor1, xor2, xor3;

	xor0 = (k[0] ^ seed) ^ k[1];
	xor1 = k[2] ^ k[3];
	xor2 = k[4] ^ k[5];
	xor3 = k[6] ^ k[7];

	xor0 ^= xor1;
	xor2 ^= xor3;

	xor0 ^= xor2;

	return (xor0 >> 32) ^ xor0;
}

#if defined(RTE_ARCH_X86_64)

#include <x86intrin.h>

static inline uint64_t
hash_crc_key8(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t crc0;

	crc0 = _mm_crc32_u64(seed, k[0]);

	return crc0;
}

static inline uint64_t
hash_crc_key16(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, crc0, crc1;

	k0 = k[0];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key24(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, crc0, crc1;

	k0 = k[0];
	k2 = k[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc0 = _mm_crc32_u64(crc0, k2);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key32(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0];
	k2 = k[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = k2 >> 32;

	crc0 = _mm_crc32_u64(crc0, crc1);
	crc1 = _mm_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key40(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, crc0, crc1, crc2, crc3;

	k0 = k[0];
	k2 = k[2];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4]);

	crc0 = _mm_crc32_u64(crc0, crc1);
	crc1 = _mm_crc32_u64(crc2, crc3);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key48(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3;

	k0 = k[0];
	k2 = k[2];
	k5 = k[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4]);

	crc0 = _mm_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = _mm_crc32_u64(crc3, k5);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key56(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0];
	k2 = k[2];
	k5 = k[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4]);

	crc4 = _mm_crc32_u64(k5, k[6]);
	crc5 = k5 >> 32;

	crc0 = _mm_crc32_u64(crc0, (crc1 << 32) ^ crc2);
	crc1 = _mm_crc32_u64(crc3, (crc4 << 32) ^ crc5);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key64(void *key, __rte_unused uint32_t key_size, uint64_t seed)
{
	uint64_t *k = key;
	uint64_t k0, k2, k5, crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0];
	k2 = k[2];
	k5 = k[5];

	crc0 = _mm_crc32_u64(k0, seed);
	crc1 = _mm_crc32_u64(k0 >> 32, k[1]);

	crc2 = _mm_crc32_u64(k2, k[3]);
	crc3 = _mm_crc32_u64(k2 >> 32, k[4]);

	crc4 = _mm_crc32_u64(k5, k[6]);
	crc5 = _mm_crc32_u64(k5 >> 32, k[7]);

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
