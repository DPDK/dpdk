/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Stephen Hemminger
 */

#ifndef _RTE_CMP_GENERIC_H_
#define _RTE_CMP_GENERIC_H_

/* Function to compare 16 byte keys */
static inline int
rte_hash_k16_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
#ifdef RTE_ARCH_64
	const unaligned_uint64_t *k1 = key1;
	const unaligned_uint64_t *k2 = key2;

	return !!((k1[0] ^ k2[0]) | (k1[1] ^ k2[1]));
#else
	const unaligned_uint32_t *k1 = key1;
	const unaligned_uint32_t *k2 = key2;

	return !!((k1[0] ^ k2[0]) | (k1[1] ^ k2[1]) |
		  (k1[2] ^ k2[2]) | (k1[3] ^ k2[3]));
#endif
}

/* Function to compare 32 byte keys */
static inline int
rte_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) |
		rte_hash_k16_cmp_eq((const uint8_t *) key1 + 16,
				(const uint8_t *) key2 + 16, key_len);
}

#endif /* _RTE_CMP_GENERIC_H_ */
