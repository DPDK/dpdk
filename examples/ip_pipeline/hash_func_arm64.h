/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Linaro Limited.
 */
#ifndef __HASH_FUNC_ARM64_H__
#define __HASH_FUNC_ARM64_H__

#define _CRC32CX(crc, val)	\
	__asm__("crc32cx %w[c], %w[c], %x[v]":[c] "+r" (crc):[v] "r" (val))

static inline uint64_t
hash_crc_key8(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key;
	uint64_t *m = mask;
	uint32_t crc0;

	crc0 = seed;
	_CRC32CX(crc0, k[0] & m[0]);

	return crc0;
}

static inline uint64_t
hash_crc_key16(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0;
	uint64_t *m = mask;
	uint32_t crc0, crc1;

	k0 = k[0] & m[0];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key24(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2;
	uint64_t *m = mask;
	uint32_t crc0, crc1;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	_CRC32CX(crc0, k2);

	crc0 ^= crc1;

	return crc0;
}

static inline uint64_t
hash_crc_key32(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2;
	uint64_t *m = mask;
	uint32_t crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc2 = k2;
	_CRC32CX(crc2, k[3] & m[3]);
	crc3 = k2 >> 32;

	_CRC32CX(crc0, crc1);
	_CRC32CX(crc2, crc3);

	crc0 ^= crc2;

	return crc0;
}

static inline uint64_t
hash_crc_key40(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2;
	uint64_t *m = mask;
	uint32_t crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc2 = k2;
	_CRC32CX(crc2, k[3] & m[3]);
	crc3 = k2 >> 32;
	_CRC32CX(crc3, k[4] & m[4]);

	_CRC32CX(crc0, crc1);
	_CRC32CX(crc2, crc3);

	crc0 ^= crc2;

	return crc0;
}

static inline uint64_t
hash_crc_key48(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2, k5;
	uint64_t *m = mask;
	uint32_t crc0, crc1, crc2, crc3;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc2 = k2;
	_CRC32CX(crc2, k[3] & m[3]);
	crc3 = k2 >> 32;
	_CRC32CX(crc3, k[4] & m[4]);

	_CRC32CX(crc0, ((uint64_t)crc1 << 32) ^ crc2);
	_CRC32CX(crc3, k5);

	crc0 ^= crc3;

	return crc0;
}

static inline uint64_t
hash_crc_key56(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2, k5;
	uint64_t *m = mask;
	uint32_t crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc2 = k2;
	_CRC32CX(crc2, k[3] & m[3]);
	crc3 = k2 >> 32;
	_CRC32CX(crc3, k[4] & m[4]);

	crc4 = k5;
	 _CRC32CX(crc4, k[6] & m[6]);
	crc5 = k5 >> 32;

	_CRC32CX(crc0, ((uint64_t)crc1 << 32) ^ crc2);
	_CRC32CX(crc3, ((uint64_t)crc4 << 32) ^ crc5);

	crc0 ^= crc3;

	return crc0;
}

static inline uint64_t
hash_crc_key64(void *key, void *mask, __rte_unused uint32_t key_size,
	uint64_t seed)
{
	uint64_t *k = key, k0, k2, k5;
	uint64_t *m = mask;
	uint32_t crc0, crc1, crc2, crc3, crc4, crc5;

	k0 = k[0] & m[0];
	k2 = k[2] & m[2];
	k5 = k[5] & m[5];

	crc0 = k0;
	_CRC32CX(crc0, seed);
	crc1 = k0 >> 32;
	_CRC32CX(crc1, k[1] & m[1]);

	crc2 = k2;
	_CRC32CX(crc2, k[3] & m[3]);
	crc3 = k2 >> 32;
	_CRC32CX(crc3, k[4] & m[4]);

	crc4 = k5;
	 _CRC32CX(crc4, k[6] & m[6]);
	crc5 = k5 >> 32;
	_CRC32CX(crc5, k[7] & m[7]);

	_CRC32CX(crc0, ((uint64_t)crc1 << 32) ^ crc2);
	_CRC32CX(crc3, ((uint64_t)crc4 << 32) ^ crc5);

	crc0 ^= crc3;

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

#endif
