/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

/* Functions to compare multiple of 16 byte keys (up to 128 bytes) */
static int
rte_hash_k16_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
	const __m128i k1 = _mm_loadu_si128((const __m128i *) key1);
	const __m128i k2 = _mm_loadu_si128((const __m128i *) key2);
	const __m128i x = _mm_xor_si128(k1, k2);

	return !_mm_test_all_zeros(x, x);
}

static int
rte_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len);
}

static int
rte_hash_k48_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
rte_hash_k64_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k32_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static int
rte_hash_k80_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
rte_hash_k96_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static int
rte_hash_k112_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 96,
				(const char *) key2 + 96, key_len);
}

static int
rte_hash_k128_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k64_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}
