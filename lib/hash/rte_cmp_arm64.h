/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Cavium, Inc
 */

/* Functions to compare multiple of 16 byte keys */
static inline int
rte_hash_k16_cmp_eq(const void *key1, const void *key2,
		    size_t key_len __rte_unused)
{
	uint64_t x0, x1, y0, y1;

	asm volatile(
		"ldp %x[x1], %x[x0], [%x[p1]]"
		: [x1]"=r"(x1), [x0]"=r"(x0)
		: [p1]"r"(key1)
		);
	asm volatile(
		"ldp %x[y1], %x[y0], [%x[p2]]"
		: [y1]"=r"(y1), [y0]"=r"(y0)
		: [p2]"r"(key2)
		);
	x0 ^= y0;
	x1 ^= y1;
	return !(x0 == 0 && x1 == 0);
}

static inline int
rte_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
	return rte_hash_k16_cmp_eq(key1, key2, 16) |
		rte_hash_k16_cmp_eq((const uint8_t *)key1 + 16,
				    (const uint8_t *)key2 + 16, 16);
}
