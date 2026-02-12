/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 Intel Corporation
 */

#include <riscv_vector.h>

/* Functions to compare multiple of 16 byte keys (up to 128 bytes) */
static inline int
rte_hash_k16_cmp_eq(const void *key1, const void *key2, size_t key_len __rte_unused)
{
    const uint8_t *p1 = (const uint8_t *)key1;
    const uint8_t *p2 = (const uint8_t *)key2;
    size_t offset = 0;

    while (offset < 16) {
        size_t vl = __riscv_vsetvl_e8m1(16 - offset);

        vuint8m1_t v1 = __riscv_vle8_v_u8m1(p1 + offset, vl);
        vuint8m1_t v2 = __riscv_vle8_v_u8m1(p2 + offset, vl);

        /* find != bytes */
        vbool8_t neq = __riscv_vmsne_vv_u8m1_b8(v1, v2, vl);

        /* if any byte mismatches ¡ú not equal */
        if (__riscv_vfirst_m_b8(neq, vl) >= 0)
            return 1;

        offset += vl;
    }

    /* all bytes equal */
    return 0;
}

static inline int
rte_hash_k32_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len);
}

static inline int
rte_hash_k48_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k16_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 16,
				(const char *) key2 + 16, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static inline int
rte_hash_k64_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k32_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 32,
				(const char *) key2 + 32, key_len);
}

static inline int
rte_hash_k80_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static inline int
rte_hash_k96_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}

static inline int
rte_hash_k112_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k32_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len) ||
		rte_hash_k16_cmp_eq((const char *) key1 + 96,
				(const char *) key2 + 96, key_len);
}

static inline int
rte_hash_k128_cmp_eq(const void *key1, const void *key2, size_t key_len)
{
	return rte_hash_k64_cmp_eq(key1, key2, key_len) ||
		rte_hash_k64_cmp_eq((const char *) key1 + 64,
				(const char *) key2 + 64, key_len);
}
